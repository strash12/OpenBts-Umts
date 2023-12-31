/**@file TransactionTable and related classes. */

/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2010 Free Software Foundation, Inc.
 * Copyright 2010 Kestrel Signal Process, Inc.
 * Copyright 2011, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

#include <UMTSCommon.h>
#include <UMTSLogicalChannel.h>
#include "TransactionTable.h"
#include "ControlCommon.h"

#include <GSML3Message.h>
#include <GSML3CCMessages.h>
#include <GSML3RRMessages.h>
#include <GSML3MMMessages.h>

#include <sqlite3.h>
#include <sqlite3util.h>

#include <SIPEngine.h>
#include <SIPInterface.h>

#include <CallControl.h>

#include <Logger.h>
#undef WARNING


using namespace std;
using namespace Control;
using namespace SIP;

using namespace UMTS;


static const char* createTransactionTable = {
	"CREATE TABLE IF NOT EXISTS TRANSACTION_TABLE ("
		"ID INTEGER PRIMARY KEY, "		// internal transaction ID
		"CHANNEL TEXT DEFAULT NULL,"	// channel description string (cross-refs CHANNEL_TABLE)
		"CREATED INTEGER NOT NULL, "	// Unix time of record creation
		"CHANGED INTEGER NOT NULL, "	// time of last state change
		"TYPE TEXT, "					// transaction type
		"SUBSCRIBER TEXT, "				// IMSI, if known
		"L3TI INTEGER, "				// GSM L3 transaction ID, +0x08 if generated by MS
		"SIP_CALLID TEXT, "				// SIP-side call id tag
		"SIP_PROXY TEXT, "				// SIP proxy IP
		"CALLED TEXT, "					// called party number
		"CALLING TEXT, "				// calling party number
		"GSMSTATE TEXT, "				// GSM/Q.931 state
		"SIPSTATE TEXT "				// SIP state
		// TODO -- Add more details from the SIP world.
	")"
};




void TransactionEntry::initTimers()
{
	// Call this only once.
	// TODO -- It would be nice if these were all configurable.
	assert(mTimers.size()==0);
	mTimers["301"] = GSM::Z100Timer(T301ms);
	mTimers["302"] = GSM::Z100Timer(T302ms);
	mTimers["303"] = GSM::Z100Timer(T303ms);
	mTimers["304"] = GSM::Z100Timer(T304ms);
	mTimers["305"] = GSM::Z100Timer(T305ms);
	mTimers["308"] = GSM::Z100Timer(T308ms);
	mTimers["310"] = GSM::Z100Timer(T310ms);
	mTimers["313"] = GSM::Z100Timer(T313ms);
	mTimers["3113"] = GSM::Z100Timer(gConfig.getNum("GSM.Timer.T3113"));
	mTimers["TR1M"] = GSM::Z100Timer(GSM::TR1Mms);
}





// Form for MT transactions.
TransactionEntry::TransactionEntry(
	const char* proxy,
	const GSM::L3MobileIdentity& wSubscriber, 
	UMTS::LogicalChannel* wChannel,
	const GSM::L3CMServiceType& wService,
	const GSM::L3CallingPartyBCDNumber& wCalling,
	GSM::CallState wState,
	const char *wMessage)
	:mID(gTransactionTable.newID()),
	mSubscriber(wSubscriber),mService(wService),
	mL3TI(gTMSITable.nextL3TI(wSubscriber.digits())),
	mCalling(wCalling),
	mSIP(proxy,mSubscriber.digits()),
	mGSMState(wState),
	mNumSQLTries(gConfig.getNum("Control.NumSQLTries")),
	mChannel(wChannel),
	mTerminationRequested(false)
{
	if (wMessage) mMessage.assign(wMessage); //strncpy(mMessage,wMessage,160);
	else mMessage.assign(""); //mMessage[0]='\0';
	initTimers();
}

// Form for MOC transactions.
TransactionEntry::TransactionEntry(
	const char* proxy,
	const GSM::L3MobileIdentity& wSubscriber,
	UMTS::LogicalChannel* wChannel,
	const GSM::L3CMServiceType& wService,
	unsigned wL3TI,
	const GSM::L3CalledPartyBCDNumber& wCalled)
	:mID(gTransactionTable.newID()),
	mSubscriber(wSubscriber),mService(wService),
	mL3TI(wL3TI),
	mCalled(wCalled),
	mSIP(proxy,mSubscriber.digits()),
	mGSMState(GSM::MOCInitiated),
	mNumSQLTries(gConfig.getNum("Control.NumSQLTries")),
	mChannel(wChannel),
	mTerminationRequested(false)
{
	assert(mSubscriber.type()==GSM::IMSIType);
	mMessage.assign(""); //mMessage[0]='\0';
	initTimers();
}


// Form for SOS transactions.
TransactionEntry::TransactionEntry(
	const char* proxy,
	const GSM::L3MobileIdentity& wSubscriber,
	UMTS::LogicalChannel* wChannel,
	const GSM::L3CMServiceType& wService,
	unsigned wL3TI)
	:mID(gTransactionTable.newID()),
	mSubscriber(wSubscriber),mService(wService),
	mL3TI(wL3TI),
	mSIP(proxy,mSubscriber.digits()),
	mGSMState(GSM::MOCInitiated),
	mNumSQLTries(2*gConfig.getNum("Control.NumSQLTries")),
	mChannel(wChannel),
	mTerminationRequested(false)
{
	mMessage.assign(""); //mMessage[0]='\0';
	initTimers();
}


// Form for MO-SMS transactions.
TransactionEntry::TransactionEntry(
	const char* proxy,
	const GSM::L3MobileIdentity& wSubscriber,
	UMTS::LogicalChannel* wChannel,
	const GSM::L3CalledPartyBCDNumber& wCalled,
	const char* wMessage)
	:mID(gTransactionTable.newID()),
	mSubscriber(wSubscriber),
	mService(GSM::L3CMServiceType::ShortMessage),
	mL3TI(7),mCalled(wCalled),
	mSIP(proxy,mSubscriber.digits()),
	mGSMState(GSM::SMSSubmitting),
	mNumSQLTries(gConfig.getNum("Control.NumSQLTries")),
	mChannel(wChannel),
	mTerminationRequested(false)
{
	assert(mSubscriber.type()==GSM::IMSIType);
	if (wMessage!=NULL) mMessage.assign(wMessage); //strncpy(mMessage,wMessage,160);
	else mMessage.assign(""); //mMessage[0]='\0';
	initTimers();
}

// Form for MO-SMS transactions with parallel call.
TransactionEntry::TransactionEntry(
	const char* proxy,
	const GSM::L3MobileIdentity& wSubscriber,
	UMTS::LogicalChannel* wChannel)
	:mID(gTransactionTable.newID()),
	mSubscriber(wSubscriber),
	mService(GSM::L3CMServiceType::ShortMessage),
	mL3TI(7),
	mSIP(proxy,mSubscriber.digits()),
	mGSMState(GSM::SMSSubmitting),
	mNumSQLTries(gConfig.getNum("Control.NumSQLTries")),
	mChannel(wChannel),
	mTerminationRequested(false)
{
	assert(mSubscriber.type()==GSM::IMSIType);
	mMessage[0]='\0';
	initTimers();
}



TransactionEntry::~TransactionEntry()
{
	ScopedLock lock(mLock);

	// Delete the SQL table entry.
	char query[100];
	sprintf(query,"DELETE FROM TRANSACTION_TABLE WHERE ID=%u",mID);
	runQuery(query);

}


bool TransactionEntry::timerExpired(const char* name) const
{
	TimerTable::const_iterator itr = mTimers.find(name);
	assert(itr!=mTimers.end());
	ScopedLock lock(mLock);
	return (itr->second).expired();
}


bool TransactionEntry::anyTimerExpired() const
{
	ScopedLock lock(mLock);
	TimerTable::const_iterator itr = mTimers.begin();
	while (itr!=mTimers.end()) {
		if ((itr->second).expired()) {
			LOG(INFO) << itr->first << " expired in " << *this;
			return true;
		}
		++itr;
	}
	return false;
}


void TransactionEntry::resetTimers()
{
	ScopedLock lock(mLock);
	TimerTable::iterator itr = mTimers.begin();
	while (itr!=mTimers.end()) {
		(itr->second).reset();
		++itr;
	}
}



bool TransactionEntry::dead() const
{
	ScopedLock lock(mLock);

	// Null state?
//	if (mGSMState==GSM::NullState) {
//		if (mSIP.state()==SIP::Cleared) return true;
//		if (mSIP.state()==SIP::Fail) return true;
//	}
	
	// Paging timed out?
	if (mGSMState==GSM::Paging) {
		TimerTable::const_iterator itr = mTimers.find("3113");
		assert(itr!=mTimers.end());
		return (itr->second).expired();
	}

	return false;
}



ostream& Control::operator<<(ostream& os, const TransactionEntry& entry)
{
	entry.text(os);
	return os;
}



void TransactionEntry::text(ostream& os) const
{
	ScopedLock lock(mLock);
	os << mID;
	if (mChannel) os << " " << *mChannel;
	else os << " no chan";
	os << " " << mSubscriber;
	os << " L3TI=" << mL3TI;
	os << " SIP-call-id=" << mSIP.callID();
	os << " SIP-proxy=" << mSIP.proxyIP() << ":" << mSIP.proxyPort();
	os << " " << mService;
	if (mCalled.digits()[0]) os << " to=" << mCalled.digits();
	if (mCalling.digits()[0]) os << " from=" << mCalling.digits();
	os << " GSMState=" << mGSMState;
	os << " SIPState=" << mSIP.state();
	os << " (" << (stateAge()+500)/1000 << " sec)";
	if (mMessage[0]) os << " message=\"" << mMessage << "\"";
}

void TransactionEntry::message(const char *wMessage, size_t length)
{
	/*if (length>520) {
		LOG(NOTICE) << "truncating long message: " << wMessage;
		length=520;
	}*/
	ScopedLock lock(mLock);
	//memcpy(mMessage,wMessage,length);
	//mMessage[length]='\0';
	mMessage.assign(wMessage, length);
}

void TransactionEntry::messageType(const char *wContentType)
{
	ScopedLock lock(mLock);
	mContentType.assign(wContentType);
}



void TransactionEntry::runQuery(const char* query) const
{
	for (unsigned i=0; i<mNumSQLTries; i++) {
		if (sqlite3_command(gTransactionTable.DB(),query)) return;
	}
	LOG(ALERT) << "transaction table access failed after " << mNumSQLTries << "attempts. query:" << query << " error: " << sqlite3_errmsg(gTransactionTable.DB());
}



void TransactionEntry::insertIntoDatabase()
{
	// This should be called only from gTransactionTable::add.

	ostringstream serviceTypeSS;
	serviceTypeSS << mService;

	ostringstream sipStateSS;
	sipStateSS << mSIP.state();
	mPrevSIPState = mSIP.state();

	char subscriber[25];
	switch (mSubscriber.type()) {
		case GSM::IMSIType: sprintf(subscriber,"IMSI%s",mSubscriber.digits()); break;
		case GSM::IMEIType: sprintf(subscriber,"IMEI%s",mSubscriber.digits()); break;
		case GSM::TMSIType: sprintf(subscriber,"TMSI%x",mSubscriber.TMSI()); break;
		default:
			sprintf(subscriber,"invalid");
			LOG(ERR) << "non-valid subscriber ID in transaction table: " << mSubscriber;
	}

	const char* stateString = GSM::CallStateString(mGSMState);
	assert(stateString);

	char query[500];
	unsigned now = (unsigned)time(NULL);
	sprintf(query,"INSERT INTO TRANSACTION_TABLE "
		        "(ID,CREATED,CHANGED,TYPE,SUBSCRIBER,L3TI,CALLED,CALLING,GSMSTATE,SIPSTATE,SIP_CALLID,SIP_PROXY) "
		"VALUES  (%u,%u,     %u,     '%s','%s',      %u,'%s',  '%s',   '%s',    '%s',      '%s',      '%s')",
		mID,now,now,
		serviceTypeSS.str().c_str(),
		subscriber,
		mL3TI,
		mCalled.digits(),
		mCalling.digits(),
		stateString,
		sipStateSS.str().c_str(),
		mSIP.callID().c_str(),
		mSIP.proxyIP().c_str()
	);

	runQuery(query);

	if (!mChannel) return;
	sprintf(query,"UPDATE TRANSACTION_TABLE SET CHANNEL='%s' WHERE ID=%u",
			mChannel->descriptiveString(), mID);
	runQuery(query);
}



void TransactionEntry::channel(UMTS::LogicalChannel* wChannel)
{
	ScopedLock lock(mLock);
	mChannel = wChannel;

	char query[500];
	if (mChannel) {
		sprintf(query,"UPDATE TRANSACTION_TABLE SET CHANGED=%u,CHANNEL='%s' WHERE ID=%u",
				(unsigned)time(NULL), mChannel->descriptiveString(), mID);
	} else {
		sprintf(query,"UPDATE TRANSACTION_TABLE SET CHANGED=%u,CHANNEL=NULL WHERE ID=%u",
				(unsigned)time(NULL), mID);
	}

	runQuery(query);
}


void TransactionEntry::GSMState(GSM::CallState wState)
{
	ScopedLock lock(mLock);
	mStateTimer.now();
	unsigned now = mStateTimer.sec();

	mGSMState = wState;
	const char* stateString = GSM::CallStateString(wState);
	assert(stateString);

	char query[150];
	sprintf(query,
		"UPDATE TRANSACTION_TABLE SET GSMSTATE='%s',CHANGED=%u WHERE ID=%u",
		stateString,now, mID);
	runQuery(query);
}


SIP::SIPState TransactionEntry::echoSIPState(SIP::SIPState state) const
{
	// Caller should hold mLock.
	if (mPrevSIPState==state) return state;
	mPrevSIPState = state;

	const char* stateString = SIP::SIPStateString(state);
	assert(stateString);

	unsigned now = time(NULL);

	char query[150];
	sprintf(query,
		"UPDATE TRANSACTION_TABLE SET SIPSTATE='%s',CHANGED=%u WHERE ID=%u",
		stateString,now,mID);
	runQuery(query);

	return state;
}




SIP::SIPState TransactionEntry::MOCSendINVITE(const char* calledUser, const char* calledDomain, short rtpPort, unsigned codec)
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MOCSendINVITE(calledUser,calledDomain,rtpPort,codec);
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MOCResendINVITE()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MOCResendINVITE();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MOCWaitForOK()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MOCWaitForOK();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MOCSendACK()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MOCSendACK();
	echoSIPState(state);
	return state;
}


SIP::SIPState TransactionEntry::MTCSendTrying()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MTCSendTrying();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MTCSendRinging()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MTCSendRinging();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MTCWaitForACK()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MTCWaitForACK();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MTCCheckForCancel()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MTCCheckForCancel();
	echoSIPState(state);
	return state;
}


SIP::SIPState TransactionEntry::MTCSendOK(short rtpPort, unsigned codec)
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MTCSendOK(rtpPort,codec);
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MODSendBYE()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MODSendBYE();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MODResendBYE()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MODResendBYE();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MODWaitForOK()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MODWaitForOK();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MTDCheckBYE()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MTDCheckBYE();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MTDSendOK()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MTDSendOK();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MOSMSSendMESSAGE(const char* calledUser, const char* calledDomain, const char* contentType)
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MOSMSSendMESSAGE(calledUser,calledDomain,mMessage.c_str(),contentType);
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MOSMSWaitForSubmit()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MOSMSWaitForSubmit();
	echoSIPState(state);
	return state;
}

SIP::SIPState TransactionEntry::MTSMSSendOK()
{
	ScopedLock lock(mLock);
	SIP::SIPState state = mSIP.MTSMSSendOK();
	echoSIPState(state);
	return state;
}

bool TransactionEntry::sendINFOAndWaitForOK(unsigned info)
{
	ScopedLock lock(mLock);
	return mSIP.sendINFOAndWaitForOK(info);
}

void TransactionEntry::SIPUser(const char* IMSI)
{
	ScopedLock lock(mLock);
	mSIP.user(IMSI);
}

void TransactionEntry::SIPUser(const char* callID, const char *IMSI , const char *origID, const char *origHost)
{
	ScopedLock lock(mLock);
	mSIP.user(callID,IMSI,origID,origHost);
}

void TransactionEntry::called(const GSM::L3CalledPartyBCDNumber& wCalled)
{
	ScopedLock lock(mLock);
	mCalled = wCalled;

	char query[151];
	snprintf(query,150,
		"UPDATE TRANSACTION_TABLE SET CALLED='%s' WHERE ID=%u",
		mCalled.digits(), mID);
	runQuery(query);
}


void TransactionEntry::L3TI(unsigned wL3TI)
{
	ScopedLock lock(mLock);
	mL3TI = wL3TI;

	char query[151];
	snprintf(query,150,
		"UPDATE TRANSACTION_TABLE SET L3TI=%u WHERE ID=%u",
		mL3TI, mID);
	runQuery(query);
}


bool TransactionEntry::terminationRequested()
{
	ScopedLock lock(mLock);
	bool retVal = mTerminationRequested;
	mTerminationRequested = false;
	return retVal;
}



TransactionTable::TransactionTable(const char* path)
	// This assumes the main application uses sdevrandom.
	:mIDCounter(random())
{
	// Connect to the database.
	int rc = sqlite3_open(path,&mDB);
	if (rc) {
		LOG(ALERT) << "Cannot open Transaction Table database at " << path << ": " << sqlite3_errmsg(mDB);
		sqlite3_close(mDB);
		mDB = NULL;
		return;
	}
	// Create a new table, if needed.
	if (!sqlite3_command(mDB,createTransactionTable)) {
		LOG(ALERT) << "Cannot create Transaction Table";
	}
	// Clear any previous entires.
	if (!sqlite3_command(gTransactionTable.DB(),"DELETE FROM TRANSACTION_TABLE"))
		LOG(WARNING) << "cannot clear previous transaction table";
}



TransactionTable::~TransactionTable()
{
	// Don't bother disposing of the memory,
	// since this is only invoked when the application exits.
	if (mDB) sqlite3_close(mDB);
}




unsigned TransactionTable::newID()
{
	ScopedLock lock(mLock);
	return mIDCounter++;
}


void TransactionTable::add(TransactionEntry* value)
{
	LOG(INFO) << "new transaction " << *value;
	ScopedLock lock(mLock);
	mTable[value->ID()]=value;
	value->insertIntoDatabase();
}



TransactionEntry* TransactionTable::find(unsigned key)
{
	// Since this is a log-time operation, we don't screw that up by calling clearDeadEntries.

	// ID==0 is a non-valid special case.
	LOG(DEBUG) << "by key: " << key;
	assert(key);
	ScopedLock lock(mLock);
	TransactionMap::iterator itr = mTable.find(key);
	if (itr==mTable.end()) return NULL;
	if (itr->second->dead()) {
		innerRemove(itr);
		return NULL;
	}
	return (itr->second);
}


void TransactionTable::innerRemove(TransactionMap::iterator itr)
{
	LOG(DEBUG) << "removing transaction: " << *(itr->second);
	gSIPInterface.removeCall(itr->second->SIPCallID());
	delete itr->second;
	mTable.erase(itr);
}


bool TransactionTable::remove(unsigned key)
{
	// ID==0 is a non-valid special case, and it shouldn't be passed here.
	if (key==0) {
		LOG(ERR) << "called with key==0";
		return false;
	}

	ScopedLock lock(mLock);
	TransactionMap::iterator itr = mTable.find(key);
	if (itr==mTable.end()) return false;
	innerRemove(itr);
	return true;
}

bool TransactionTable::removePaging(unsigned key)
{
	// ID==0 is a non-valid special case and should not be passed here.
	assert(key);
	ScopedLock lock(mLock);
	TransactionMap::iterator itr = mTable.find(key);
	if (itr==mTable.end()) return false;
	if (itr->second->GSMState()!=GSM::Paging) return false;
	innerRemove(itr);
	return true;
}




void TransactionTable::clearDeadEntries()
{
	// Caller should hold mLock.
	TransactionMap::iterator itr = mTable.begin();
	while (itr!=mTable.end()) {
		if (!itr->second->dead()) ++itr;
		else {
			LOG(DEBUG) << "erasing " << itr->first;
			TransactionMap::iterator old = itr;
			itr++;
			innerRemove(old);
		}
	}
}




TransactionEntry* TransactionTable::find(const UMTS::LogicalChannel *chan)
{
	LOG(DEBUG) << "by channel: " << *chan << " (" << chan << ")";

	// Yes, it's linear time.
	// Since clearDeadEntries is also linear, do that here, too.
	clearDeadEntries();

	// Brute force search.
	ScopedLock lock(mLock);
	for (TransactionMap::iterator itr = mTable.begin(); itr!=mTable.end(); ++itr) {
		const UMTS::LogicalChannel* thisChan = itr->second->channel();
		//LOG(DEBUG) << "looking for " << *chan << " (" << chan << ")" << ", found " << *(thisChan) << " (" << thisChan << ")";
		if ((void*)thisChan == (void*)chan) return itr->second;
	}
	//LOG(DEBUG) << "no match for " << *chan << " (" << chan << ")";
	return NULL;
}


TransactionEntry* TransactionTable::find(const GSM::L3MobileIdentity& mobileID, GSM::CallState state)
{
	LOG(DEBUG) << "by ID and state: " << mobileID << " in " << state;

	// Yes, it's linear time.
	// Since clearDeadEntries is also linear, do that here, too.
	clearDeadEntries();

	// Brtue force search.
	ScopedLock lock(mLock);
	for (TransactionMap::iterator itr = mTable.begin(); itr!=mTable.end(); ++itr) {
		if (itr->second->GSMState() != state) continue;
		if (itr->second->subscriber() == mobileID) return itr->second;
	}
	return NULL;
}


TransactionEntry* TransactionTable::find(const GSM::L3MobileIdentity& mobileID, const char* callID)
{
	assert(callID);
	LOG(DEBUG) << "by ID and call-ID: " << mobileID << ", call " << callID;

	string callIDString = string(callID);
	// Yes, it's linear time.
	// Since clearDeadEntries is also linear, do that here, too.
	clearDeadEntries();

	// Brtue force search.
	ScopedLock lock(mLock);
	for (TransactionMap::iterator itr = mTable.begin(); itr!=mTable.end(); ++itr) {
		if (itr->second->mSIP.callID() != callIDString) continue;
		if (itr->second->subscriber() == mobileID) return itr->second;
	}
	return NULL;
}


TransactionEntry* TransactionTable::answeredPaging(const GSM::L3MobileIdentity& mobileID)
{
	// Yes, it's linear time.
	// Even in a 6-ARFCN system, it should rarely be more than a dozen entries.

	// Since clearDeadEntries is also linear, do that here, too.
	clearDeadEntries();

	// Brtue force search.
	ScopedLock lock(mLock);
	for (TransactionMap::iterator itr = mTable.begin(); itr!=mTable.end(); ++itr) {
		if (itr->second->GSMState() != GSM::Paging) continue;
		if (itr->second->subscriber() == mobileID) {
			// Stop T3113 and change the state.
			itr->second->GSMState(GSM::AnsweredPaging);
			itr->second->resetTimer("3113");
			return itr->second;
		}
	}
	return NULL;
}


UMTS::LogicalChannel* TransactionTable::findChannel(const GSM::L3MobileIdentity& mobileID)
{
	// Yes, it's linear time.
	// Even in a 6-ARFCN system, it should rarely be more than a dozen entries.

	// Since clearDeadEntries is also linear, do that here, too.
	clearDeadEntries();

	// Brtue force search.
	ScopedLock lock(mLock);
	for (TransactionMap::iterator itr = mTable.begin(); itr!=mTable.end(); ++itr) {
		if (itr->second->subscriber() != mobileID) continue;
		UMTS::LogicalChannel* chan = itr->second->channel();
		if (!chan) continue;
		if (chan->type() == UMTS::DTCHType) return chan;
		if (chan->type() == UMTS::DCCHType) return chan;
	}
	return NULL;
}


unsigned TransactionTable::countChan(const UMTS::LogicalChannel* chan)
{
	ScopedLock lock(mLock);
	clearDeadEntries();
	unsigned count = 0;
	for (TransactionMap::iterator itr = mTable.begin(); itr!=mTable.end(); ++itr) {
		if (itr->second->channel() == chan) count++;
	}
	return count;
}



size_t TransactionTable::dump(ostream& os) const
{
	ScopedLock lock(mLock);
	for (TransactionMap::const_iterator itr = mTable.begin(); itr!=mTable.end(); ++itr) {
		os << *(itr->second) << endl;
	}
	return mTable.size();
}




// vim: ts=4 sw=4
