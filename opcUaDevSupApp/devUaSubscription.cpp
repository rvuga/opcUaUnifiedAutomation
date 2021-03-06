/*************************************************************************\
* Copyright (c) 2016 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH (HZB), Berlin, Germany.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU Lesser General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
\*************************************************************************/

#include "uasubscription.h"
#include "uasession.h"
#include <epicsTypes.h>
#include <epicsPrint.h>
#include <epicsTime.h>
#include "dbScan.h"
#include "devOpcUa.h"
#include "drvOpcUa.h"
#include "devUaSubscription.h"

DevUaSubscription::DevUaSubscription(int debug=0)
    : debug(debug)
{}

DevUaSubscription::~DevUaSubscription()
{
    deleteSubscription();
}

void DevUaSubscription::subscriptionStatusChanged(
    OpcUa_UInt32      clientSubscriptionHandle,
    const UaStatus&   status)
{
    OpcUa_ReferenceParameter(clientSubscriptionHandle); // We use the callback only for this subscription
    errlogPrintf("DevUaSubscription: subscription no longer valid - failed with status %d (%s)\n",
                 status.statusCode(),
                 status.toString().toUtf8());
}

void DevUaSubscription::dataChange(
    OpcUa_UInt32               clientSubscriptionHandle,
    const UaDataNotifications& dataNotifications,
    const UaDiagnosticInfos&   diagnosticInfos)
{
    OpcUa_ReferenceParameter(clientSubscriptionHandle); // We use the callback only for this subscription
    OpcUa_ReferenceParameter(diagnosticInfos);
    OpcUa_UInt32 i = 0;
    char timeBuf[30];
    getTime(timeBuf);
    if(debug>2) errlogPrintf("dataChange %s\n",timeBuf);
    for ( i=0; i<dataNotifications.length(); i++ )
    {
        struct dataChangeError {};
        OPCUA_ItemINFO* pOPCUA_ItemINFO = m_vectorUaItemInfo->at(dataNotifications[i].ClientHandle);
        if(debug>3)
            errlogPrintf("\t%s\n",pOPCUA_ItemINFO->prec->name);
        else if(pOPCUA_ItemINFO->debug >= 2)
            errlogPrintf("dataChange: %s %s\n",timeBuf,pOPCUA_ItemINFO->prec->name);
        epicsMutexLock(pOPCUA_ItemINFO->flagLock);
        try {
            if (OpcUa_IsBad(dataNotifications[i].Value.StatusCode) )
            {
                if(debug || (pOPCUA_ItemINFO->debug>= 3)) errlogPrintf("%s %s Variable %d failed with status %s\n",timeBuf ,pOPCUA_ItemINFO->prec->name,dataNotifications[i].ClientHandle,
                       UaStatus(dataNotifications[i].Value.StatusCode).toString().toUtf8());
                throw dataChangeError();
            }
            pOPCUA_ItemINFO->stat = 0;
            UaVariant val = dataNotifications[i].Value.Value;

            if(val.isArray()){
                UaByteArray   aByte;
                UaInt16Array  aInt16;
                UaUInt16Array aUInt16;
                UaInt32Array  aInt32;
                UaUInt32Array aUInt32;
                UaFloatArray  aFloat;
                UaDoubleArray aDouble;
                if(pOPCUA_ItemINFO->debug >= 2) errlogPrintf("dataChange %s\t Array\n", pOPCUA_ItemINFO->prec->name);

                if(val.arraySize() <= pOPCUA_ItemINFO->arraySize) {
                    switch(pOPCUA_ItemINFO->recDataType) {
                    case epicsInt8T:
                    case epicsUInt8T:
                        val.toByteArray( aByte);
                        memcpy(pOPCUA_ItemINFO->pRecVal,aByte.data(),sizeof(epicsInt8)*pOPCUA_ItemINFO->arraySize);
                        break;
                    case epicsInt16T:
                        val.toInt16Array( aInt16);
                        memcpy(pOPCUA_ItemINFO->pRecVal,aInt16.rawData(),sizeof(epicsInt16)*pOPCUA_ItemINFO->arraySize);
                        break;
                    case epicsEnum16T:
                    case epicsUInt16T:
                        val.toUInt16Array( aUInt16);
                        memcpy(pOPCUA_ItemINFO->pRecVal,aUInt16.rawData(),sizeof(epicsUInt16)*pOPCUA_ItemINFO->arraySize);
                        break;
                    case epicsInt32T:
                        val.toInt32Array( aInt32);
                        memcpy(pOPCUA_ItemINFO->pRecVal,aInt32.rawData(),sizeof(epicsInt32)*pOPCUA_ItemINFO->arraySize);
                        break;
                    case epicsUInt32T:
                        val.toUInt32Array( aUInt32);
                        memcpy(pOPCUA_ItemINFO->pRecVal,aUInt32.rawData(),sizeof(epicsUInt32)*pOPCUA_ItemINFO->arraySize);
                        break;
                    case epicsFloat32T:
                        val.toFloatArray( aFloat);
                        memcpy(pOPCUA_ItemINFO->pRecVal,aFloat.rawData(),sizeof(epicsFloat32)*pOPCUA_ItemINFO->arraySize);
                        break;
                    case epicsFloat64T:
                        val.toDoubleArray( aDouble);
                        memcpy(pOPCUA_ItemINFO->pRecVal,aDouble.rawData(),sizeof(epicsFloat64)*pOPCUA_ItemINFO->arraySize);
                        break;
                    default:
                        if(pOPCUA_ItemINFO->debug >= 2) errlogPrintf("%s dataChange: Can't convert data type\n",pOPCUA_ItemINFO->prec->name);
                        throw dataChangeError();
                    }
                }
                else {
                    if(debug || (pOPCUA_ItemINFO->debug>= 2)) errlogPrintf("%s %s dataChange Error Record arraysize %d < OpcItem Size %d\n",timeBuf, pOPCUA_ItemINFO->prec->name,val.arraySize(),pOPCUA_ItemINFO->arraySize);
                    throw dataChangeError();
                }
                if(pOPCUA_ItemINFO->prec->scan == SCAN_IO_EVENT)
                {
                    scanIoRequest( pOPCUA_ItemINFO->ioscanpvt );    // Update the record immediatly, for scan>SCAN_IO_EVENT update by periodic scan.
                }
            }      // end array
            else { // is no array
                if(pOPCUA_ItemINFO->inpDataType) { // is OUT Record
                    if(pOPCUA_ItemINFO->debug>= 3) errlogPrintf("\tOUT rec noOut:%d ",pOPCUA_ItemINFO->noOut);
                    if(pOPCUA_ItemINFO->noOut==0) {     // Means: dataChange by external value change. Set Record! Invoke processing by callback but suppress another write operation
                        pOPCUA_ItemINFO->noOut = 1;
                        switch(pOPCUA_ItemINFO->inpDataType){ // Write direct to the records VAL field
                            case epicsInt32T:
                                val.toInt32( *((OpcUa_Int32*)pOPCUA_ItemINFO->pInpVal) );
                                if(pOPCUA_ItemINFO->debug >= 3) errlogPrintf("\tepicsInt32 recVal: %d\n",*((epicsInt32*)pOPCUA_ItemINFO->pRecVal));
                                break;
                            case epicsUInt32T:
                                val.toUInt32( *((OpcUa_UInt32*)pOPCUA_ItemINFO->pInpVal));
                                if(pOPCUA_ItemINFO->debug >= 3) errlogPrintf("\tepicsUInt32 recVal: %u\n",*((epicsUInt32*)pOPCUA_ItemINFO->pRecVal));
                                break;
                            case epicsFloat64T:
                                val.toDouble( *((epicsFloat64*)pOPCUA_ItemINFO->pInpVal) );
                                if(pOPCUA_ItemINFO->debug >= 3) errlogPrintf("\tepicsFloat64 recVal: %lf\n",*((epicsFloat64*)pOPCUA_ItemINFO->pRecVal));
                                break;
                            case epicsOldStringT:
                                strncpy((char*)pOPCUA_ItemINFO->pInpVal,val.toString().toUtf8(),MAX_STRING_SIZE);    // string length: see epicsTypes.h
                                if(pOPCUA_ItemINFO->debug >= 3) errlogPrintf("\tepicsOldStringT opcVal: '%s'\n",(pOPCUA_ItemINFO->varVal).cString);
                                break;
                        default:
                            if(debug || (pOPCUA_ItemINFO->debug>= 2)) errlogPrintf("%s %s\tdataChange: unsupported recDataType '%s'\n",timeBuf,pOPCUA_ItemINFO->prec->name,epicsTypeNames[pOPCUA_ItemINFO->recDataType]);
                            throw dataChangeError();
                        }
                        callbackRequest(&(pOPCUA_ItemINFO->callback)); // out-records are SCAN="passive" so scanIoRequest doesn't work
                    }
                    else {  // Means dataChange after write operation of the record. Ignore this, no callback, suppress another processing of the record
                        pOPCUA_ItemINFO->noOut=0;
                    }
                }
                else { // is IN Record
                    if(pOPCUA_ItemINFO->debug >= 2) errlogPrintf("dataChange %s\tIN rec noOut:%d\n", pOPCUA_ItemINFO->prec->name,pOPCUA_ItemINFO->noOut);
                    switch(pOPCUA_ItemINFO->recDataType){ // Write to the OPCUA_ItemINFO variant, record processing will get the value when processing
                        case epicsInt32T:
                            val.toInt32( reinterpret_cast<OpcUa_Int32&>( (pOPCUA_ItemINFO->varVal).Int32 ) );
                            if(pOPCUA_ItemINFO->debug >= 3) errlogPrintf("\tepicsInt32 opcVal: %d\n",(pOPCUA_ItemINFO->varVal).Int32);
                            break;
                        case epicsUInt32T:
                            val.toUInt32( reinterpret_cast<OpcUa_UInt32&>( (pOPCUA_ItemINFO->varVal).UInt32) );
                            if(pOPCUA_ItemINFO->debug >= 3) errlogPrintf("\tepicsUInt32 opcVal: %u\n",(pOPCUA_ItemINFO->varVal).UInt32);
                            break;
                        case epicsFloat64T:
                            val.toDouble( (pOPCUA_ItemINFO->varVal).Double);
                            if(pOPCUA_ItemINFO->debug >= 3) errlogPrintf("\tepicsFloat64 opcVal: %lf\n",(pOPCUA_ItemINFO->varVal).Double);
                            break;
                        case epicsOldStringT:
                            strncpy((pOPCUA_ItemINFO->varVal).cString,val.toString().toUtf8(),MAX_STRING_SIZE);    // string length: see epicsTypes.h
                            if(pOPCUA_ItemINFO->debug >= 3) errlogPrintf("\tepicsOldString opcVal: '%s'\n",(pOPCUA_ItemINFO->varVal).cString);
                            break;
                    default:
                        if(debug || (pOPCUA_ItemINFO->debug>= 2)) errlogPrintf("%s %s\tdataChange: unsupported recDataType '%s'\n",timeBuf,pOPCUA_ItemINFO->prec->name,epicsTypeNames[pOPCUA_ItemINFO->recDataType]);
                        throw dataChangeError();
                    }
                    if(pOPCUA_ItemINFO->prec->scan == SCAN_IO_EVENT)
                    {
                        scanIoRequest( pOPCUA_ItemINFO->ioscanpvt );    // Update the record immediatly, for scan>SCAN_IO_EVENT update by periodic scan.
                    }

                }
            }
        }
        catch(dataChangeError) {
            pOPCUA_ItemINFO->stat = 1;
        }
        // I'm not shure about the posibility of another exception but of the damage it could do!
        catch(...) {
            pOPCUA_ItemINFO->stat = 1;
            if(debug || (pOPCUA_ItemINFO->debug>= 2)) errlogPrintf("%s %s\tdataChange: unexpected exception '%s'\n",timeBuf,pOPCUA_ItemINFO->prec->name,epicsTypeNames[pOPCUA_ItemINFO->recDataType]);
            pOPCUA_ItemINFO->debug = 4;
        }
        epicsMutexUnlock(pOPCUA_ItemINFO->flagLock);

        setTimestamp(pOPCUA_ItemINFO,UaDateTime(dataNotifications[i].Value.ServerTimestamp));

        if(pOPCUA_ItemINFO->debug >= 4)
                errlogPrintf("\tepicsType: %2d,%s opcType%2d:%s noOut:%d\n",
                   pOPCUA_ItemINFO->recDataType,epicsTypeNames[pOPCUA_ItemINFO->recDataType],
                   pOPCUA_ItemINFO->itemDataType,variantTypeStrings(pOPCUA_ItemINFO->itemDataType),
                   pOPCUA_ItemINFO->noOut);
    } //end for
    return;
}

void setTimestamp(OPCUA_ItemINFO * pOPCUA_ItemINFO, const UaDateTime &dt)
{
    dbCommon *prec = pOPCUA_ItemINFO->prec;

    if(prec->tse == epicsTimeEventDeviceTime ) {
        prec->time.secPastEpoch = dt.toTime_t() - POSIX_TIME_AT_EPICS_EPOCH;
        prec->time.nsec         = dt.msec()*1000000L; // msec is 100ns steps
    }
    if(pOPCUA_ItemINFO->debug >= 4) {
        char currentBuffer[30];
        char timeBuffer[30];
        epicsTimeToStrftime(timeBuffer,28,"%y-%m-%dT%H:%M:%S.%06f",&(prec->time));
        errlogPrintf("\tsetTimestamp: Curr:%s TSE:%d Server:%s recTIME:%s\n",getTime(currentBuffer),prec->tse,dt.toString().toUtf8(),timeBuffer);
    }
}

void DevUaSubscription::newEvents(
    OpcUa_UInt32                clientSubscriptionHandle,
    UaEventFieldLists&          eventFieldList)
{
    OpcUa_ReferenceParameter(clientSubscriptionHandle);
    OpcUa_ReferenceParameter(eventFieldList);
    if(debug) errlogPrintf("DevUaSubscription::newEvents called\n");
}

UaStatus DevUaSubscription::createSubscription(UaSession *pSession)
{
    m_pSession = pSession;

    UaStatus result;
    ServiceSettings serviceSettings;
    SubscriptionSettings subscriptionSettings;
    subscriptionSettings.publishingInterval = 100;
    if(debug) errlogPrintf("Creating subscription\n");
    result = pSession->createSubscription(
        serviceSettings,
        this,
        1,
        subscriptionSettings,
        OpcUa_True,
        &m_pSubscription);
    if (result.isBad())
    {
        errlogPrintf("DevUaSubscription::createSubscription failed with status %#8x (%s)\n",
                     result.statusCode(),
                     result.toString().toUtf8());
    }
    return result;
}

UaStatus DevUaSubscription::deleteSubscription()
{
    UaStatus result;
    ServiceSettings serviceSettings;
    // let the SDK cleanup the resources for the existing subscription
    if(debug) errlogPrintf("Deleting subscription\n");
    result = m_pSession->deleteSubscription(
        serviceSettings,
        &m_pSubscription);
    if (result.isBad())
    {
        errlogPrintf("DevUaSubscription::deleteSubscription failed with status %#8x (%s)\n",
                     result.statusCode(),
                     result.toString().toUtf8());
    }
    //TODO: setting the pointer NULL if delete failed might be a memory leak?
    return result;
}

UaStatus DevUaSubscription::createMonitoredItems(std::vector<UaNodeId> &vUaNodeId,std::vector<OPCUA_ItemINFO *> *uaItemInfo)
{
    if(debug) errlogPrintf("DevUaSubscription::createMonitoredItems\n");
    if( uaItemInfo->size() == vUaNodeId.size())
        m_vectorUaItemInfo = uaItemInfo;
    else
    {
        errlogPrintf("\nDevUaSubscription::createMonitoredItems Error: Nr of uaItems %i != nr of browsepathItems %i\n",(int)uaItemInfo->size(),(int)vUaNodeId.size());
        return OpcUa_BadInvalidState;
    }
    if(false == m_pSession->isConnected() ) {
        errlogPrintf("\nDevUaSubscription::createMonitoredItems Error: session not connected\n");
        return OpcUa_BadInvalidState;

    }

    UaStatus result;
    OpcUa_UInt32 i;
    ServiceSettings serviceSettings;
    UaMonitoredItemCreateRequests itemsToCreate;
    UaMonitoredItemCreateResults createResults;
    // Configure one item to add to subscription
    // We monitor the value of the ServerStatus -> CurrentTime
    itemsToCreate.create(vUaNodeId.size());
    for(i=0; i<vUaNodeId.size(); i++) {
        if ( !vUaNodeId[i].isNull() ) {
            UaNodeId tempNode(vUaNodeId[i]);
            itemsToCreate[i].ItemToMonitor.AttributeId = OpcUa_Attributes_Value;
            tempNode.copyTo(&(itemsToCreate[i].ItemToMonitor.NodeId));
            itemsToCreate[i].RequestedParameters.ClientHandle = i;
            itemsToCreate[i].RequestedParameters.SamplingInterval = 100;
            itemsToCreate[i].RequestedParameters.QueueSize = 1;
            itemsToCreate[i].RequestedParameters.DiscardOldest = OpcUa_True;
            itemsToCreate[i].MonitoringMode = OpcUa_MonitoringMode_Reporting;
        }
        else {
            errlogPrintf("%s Skip illegal node: %s\n",uaItemInfo->at(i)->prec->name,uaItemInfo->at(i)->ItemPath);
        }
    }
    if(debug) errlogPrintf("\nAdd monitored items to subscription ...\n");
    result = m_pSubscription->createMonitoredItems(
        serviceSettings,
        OpcUa_TimestampsToReturn_Both,
        itemsToCreate,
        createResults);
    if (result.isGood())
    {
        // check individual results
        for (i = 0; i < createResults.length(); i++)
        {
            if (OpcUa_IsGood(createResults[i].StatusCode))
            {
                if(debug>1) errlogPrintf("%4d: %s\n",i,
                    UaNodeId(itemsToCreate[i].ItemToMonitor.NodeId).toXmlString().toUtf8());
            }
            else
            {
                if(debug) {
                    OPCUA_ItemINFO* pOPCUA_ItemINFO = m_vectorUaItemInfo->at(i);
                    errlogPrintf("%4d %s DevUaSubscription::createMonitoredItems failed for node: %s - Status %s\n",
                        i, pOPCUA_ItemINFO->prec->name,
                        UaNodeId(itemsToCreate[i].ItemToMonitor.NodeId).toXmlString().toUtf8(),
                        UaStatus(createResults[i].StatusCode).toString().toUtf8());
                }
            }
        }
    }
    else
    {
       if(debug)  errlogPrintf("DevUaSubscription::createMonitoredItems service call failed with status %s\n", result.toString().toUtf8());
    }
    return result;
}
