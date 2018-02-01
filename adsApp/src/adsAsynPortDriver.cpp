/*
 */

#include "adsAsynPortDriver.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>

#include "cmd.h"

#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <iocsh.h>

#include <epicsExport.h>
#include <dbStaticLib.h>
#include <dbAccess.h>

#include "adsCom.h"


static void adsNotifyCallback(const AmsAddr* pAddr, const AdsNotificationHeader* pNotification, uint32_t hUser)
{
    const uint8_t* data = reinterpret_cast<const uint8_t*>(pNotification + 1);
    //std::cout << "hUser 0x" << std::hex << hUser <<
    //    " sample time: " << std::dec << pNotification->nTimeStamp <<
    //    " sample size: " << std::dec << pNotification->cbSampleSize <<
    //    " value:";
    for (size_t i = 0; i < pNotification->cbSampleSize; ++i) {
    //    std::cout << " 0x" << std::dec << (int)data[i];
    }
    //std::cout << '\n';
}

static const char *driverName="adsAsynPortDriver";

// Constructor for the adsAsynPortDriver class.
adsAsynPortDriver::adsAsynPortDriver(const char *portName,
                                     const char *ipaddr,
                                     const char *amsaddr,
                                     unsigned int amsport,
                                     int paramTableSize,
                                     unsigned int priority,
                                     int autoConnect,
                                     int noProcessEos)
                    :asynPortDriver(portName,
                                   1, /* maxAddr */
                                   paramTableSize,
                                   asynInt32Mask | asynFloat64Mask | asynFloat32ArrayMask | asynFloat64ArrayMask | asynEnumMask | asynDrvUserMask | asynOctetMask | asynInt8ArrayMask | asynInt16ArrayMask | asynInt32ArrayMask, /* Interface mask */
                                   asynInt32Mask | asynFloat64Mask | asynFloat32ArrayMask | asynFloat64ArrayMask | asynEnumMask | asynDrvUserMask | asynOctetMask | asynInt8ArrayMask | asynInt16ArrayMask | asynInt32ArrayMask,  /* Interrupt mask */
                                   ASYN_CANBLOCK, /* asynFlags.  This driver does not block and it is not multi-device, so flag is 0 */
                                   autoConnect, /* Autoconnect */
                                   priority, /* Default priority */
                                   0) /* Default stack size*/
{
  eventId_ = epicsEventCreate(epicsEventEmpty);
  setCfgData(portName,ipaddr,amsaddr,amsport,priority,autoConnect,noProcessEos);
  pAdsParamArray_= new adsParamInfo*[paramTableSize];
  memset(pAdsParamArray_,0,sizeof(*pAdsParamArray_));
  pAdsParamArrayCount_=0;
  paramTableSize_=paramTableSize;
}

adsAsynPortDriver::~adsAsynPortDriver()
{
  for(int i=0;i<pAdsParamArrayCount_;i++){
    free(pAdsParamArray_[i]->recordName);
    free(pAdsParamArray_[i]->recordType);
    free(pAdsParamArray_[i]->scan);
    free(pAdsParamArray_[i]->dtyp);
    free(pAdsParamArray_[i]->inp);
    free(pAdsParamArray_[i]->out);
    free(pAdsParamArray_[i]->drvInfo);
    free(pAdsParamArray_[i]->plcSymAdr);
    delete pAdsParamArray_[i];
  }
  delete pAdsParamArray_;
}

void adsAsynPortDriver::report(FILE *fp, int details)
{
  if (details >= 1) {
    fprintf(fp, "    Port %s\n",portName);
  }
}

asynStatus adsAsynPortDriver::disconnect(asynUser *pasynUser)
{
  const char* functionName = "disconnect";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:\n", driverName, functionName);

//  #if 0
    /* TODO: prevent a reconnect */
//    if (!(tty->flags & FLAG_CONNECT_PER_TRANSACTION) ||
//        (tty->flags & FLAG_SHUTDOWN))
//      pasynManager->exceptionDisconnect(pasynUser);
//  #endif

  int error=adsDisconnect();
  if (error){
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,"adsAsynPortDriver::disconnect: ERROR: Disconnect failed (0x%x).\n",error);
    return asynError;
  }
  return asynSuccess;
}

asynStatus adsAsynPortDriver::connectIt( asynUser *pasynUser)

{
  //epicsMutexLockStatus mutexLockStatus;
  int res;
  int connectOK;

  //(void)pasynUser;
  //mutexLockStatus = epicsMutexLock(adsController_p->mutexId);
  //if (mutexLockStatus != epicsMutexLockOK) {
  //  return asynError;
  //}

  lock();

  //if (adsController_p->connected) {
  //  epicsMutexUnlock(adsController_p->mutexId);
  //  return asynSuccess;
  //}

  /* adsConnect() returns 0 if failed */
  res = adsConnect(ipaddr_,amsaddr_,amsport_);

  connectOK =  res >= 0;
  //if (connectOK) adsController_p->connected = 1;
  //epicsMutexUnlock(adsController_p->mutexId);

  unlock();

  return connectOK ? asynSuccess : asynError;
}

asynStatus adsAsynPortDriver::connect(asynUser *pasynUser)
{
  const char* functionName = "connect";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:\n", driverName, functionName);

  asynStatus status = connectIt(pasynUser);
  if (status == asynSuccess){
    //pasynManager->exceptionConnect(pasynUser);
    asynPortDriver::connect(pasynUser);
  }

  return status;
}

asynStatus adsAsynPortDriver::drvUserCreate(asynUser *pasynUser,const char *drvInfo,const char **pptypeName,size_t *psize)
{
  const char* functionName = "drvUserCreate";

  asynPortDriver::drvUserCreate(pasynUser,drvInfo,pptypeName,psize);
  //dbAddr  paddr;
  //dbNameToAddr("ADS_IOC::GetFTest3",&paddr);

  //asynPrint(pasynUser, ASYN_TRACE_INFO,"####################################\n");
  //asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s: drvInfo=%s.\n", driverName, functionName,drvInfo);
  //const char *data;
  //size_t writeSize=1024;
  //asynStatus status=drvUserGetType(pasynUser,&data,&writeSize);
  //asynPrint(pasynUser, ASYN_TRACE_INFO,"STATUS = %d, data= %s, size=%d\n",status,data,writeSize);

  int index=0;
  asynStatus status=findParam(drvInfo,&index);
  if(status==asynSuccess){
    asynPrint(pasynUser, ASYN_TRACE_INFO, "PARAMETER INDEX FOUND AT: %d for %s. \n",index,drvInfo);
  }
  else{
   // Collect data from drvInfo string and record
    adsParamInfo *paramInfo=new adsParamInfo();
    memset(paramInfo,0,sizeof(adsParamInfo));
    status=getRecordInfoFromDrvInfo(drvInfo, paramInfo);
    if(status==asynSuccess){
      status=createParam(drvInfo,paramInfo->asynType,&index);
      if(status==asynSuccess){
        paramInfo->paramIndex=index;
        status=parsePlcInfofromDrvInfo(drvInfo,paramInfo);
        if(status==asynSuccess){
          pAdsParamArray_[pAdsParamArrayCount_]=paramInfo;
          pAdsParamArrayCount_++;
          //print all parameters
          for(int i=0; i<pAdsParamArrayCount_;i++){
            printParamInfo(pAdsParamArray_[i]);
          }
          asynPrint(pasynUser, ASYN_TRACE_INFO, "PARAMETER CREATED AT: %d for %s.\n",index,drvInfo);
        }
        else{
          asynPrint(pasynUser, ASYN_TRACE_ERROR, "FAILED PARSING OF DRVINFO: %s.\n",drvInfo);
        }
      }
      else{
        asynPrint(pasynUser, ASYN_TRACE_ERROR, "CREATE PARAMETER FAILED for %s.\n",drvInfo);
      }
    }
  }

  //asynPrint(pasynUser, ASYN_TRACE_INFO, "pasynUser->errorMessage=%s.\n",pasynUser->errorMessage);
  //asynPrint(pasynUser, ASYN_TRACE_INFO, "pasynUser->timeout=%lf.\n",pasynUser->timeout);
  //asynPrint(pasynUser, ASYN_TRACE_INFO, "pasynUser->reason=%d.\n",pasynUser->reason);
  //asynPrint(pasynUser, ASYN_TRACE_INFO, "pasynUser->auxStatus=%d.\n",pasynUser->auxStatus);
  //asynPrint(pasynUser, ASYN_TRACE_INFO, "pasynUser->alarmStatus=%d.\n",pasynUser->alarmStatus);
  //asynPrint(pasynUser, ASYN_TRACE_INFO, "pasynUser->alarmSeverity=%d.\n",pasynUser->alarmSeverity);
  //dbDumpRecords();
  //return this->drvUserCreateParam(pasynUser, drvInfo, pptypeName, psize,pParamTable_, paramTableSize_);
  //pasynManager->report(stdout,1,"ADS_1");

  return asynSuccess;
}

asynParamType adsAsynPortDriver::dtypStringToAsynType(char *dtype)
{
  if(strcmp("asynFloat64",dtype)==0){
    return asynParamFloat64;
  }
  if(strcmp("asynInt32",dtype)==0){
    return asynParamInt32;
  }
  return asynParamNotDefined;

  //  asynParamUInt32Digital,
  //  asynParamOctet,
  //  asynParamInt8Array,
  //  asynParamInt16Array,
  //  asynParamInt32Array,
  //  asynParamFloat32Array,
  //  asynParamFloat64Array,
  //  asynParamGenericPointer
}

asynStatus adsAsynPortDriver::getRecordInfoFromDrvInfo(const char *drvInfo,adsParamInfo *paramInfo)
{
  paramInfo->amsPort=amsport_;
  DBENTRY *pdbentry;
  pdbentry = dbAllocEntry(pdbbase);
  long status = dbFirstRecordType(pdbentry);
  bool recordFound=false;
  if(status) {
    dbFreeEntry(pdbentry);
    return asynError;
  }
  while(!status) {
    paramInfo->recordType=strdup(dbGetRecordTypeName(pdbentry));
    status = dbFirstRecord(pdbentry);
    while(!status) {
      paramInfo->recordName=strdup(dbGetRecordName(pdbentry));
      if(!dbIsAlias(pdbentry)){
        status=dbFindField(pdbentry,"INP");
        if(!status){
          paramInfo->inp=strdup(dbGetString(pdbentry));
          paramInfo->isInput=true;
          char port[MAX_FIELD_CHAR_LENGTH];
          int adr;
          int timeout;
          char currdrvInfo[MAX_FIELD_CHAR_LENGTH];
          int nvals=sscanf(paramInfo->inp,"@asyn(%[^,],%d,%d)%s",port,&adr,&timeout,currdrvInfo);
          if(nvals==4){
            //Ensure correct port and drvinfo
            if(strcmp(port,portName)==0 && strcmp(drvInfo,currdrvInfo)==0){
              recordFound=true;  // Correct port and drvinfo!\n");
            }
          }
        }
        else{
          paramInfo->isInput=false;
        }
        status=dbFindField(pdbentry,"OUT");
        if(!status){
          paramInfo->out=strdup(dbGetString(pdbentry));
          paramInfo->isOutput=true;
          char port[MAX_FIELD_CHAR_LENGTH];
          int adr;
          int timeout;
          char currdrvInfo[MAX_FIELD_CHAR_LENGTH];
          int nvals=sscanf(paramInfo->out,"@asyn(%[^,],%d,%d)%s",port,&adr,&timeout,currdrvInfo);
          if(nvals==4){
            //Ensure correct port and drvinfo
            if(strcmp(port,portName)==0 && strcmp(drvInfo,currdrvInfo)==0){
              recordFound=true;  // Correct port and drvinfo!\n");
            }
          }
        }
        else{
          paramInfo->isOutput=false;
        }

        if(recordFound){
          // Correct record found. Collect data from fields
          //DTYP
          status=dbFindField(pdbentry,"DTYP");
          if(!status){
            paramInfo->dtyp=strdup(dbGetString(pdbentry));
            paramInfo->asynType=dtypStringToAsynType(dbGetString(pdbentry));
          }
          else{
            paramInfo->dtyp=0;
            paramInfo->asynType=asynParamNotDefined;
          }
          //SCAN
          status=dbFindField(pdbentry,"SCAN");
          if(!status){
            paramInfo->scan=strdup(dbGetString(pdbentry));
          }
          else{
            paramInfo->scan=0;
          }
          //drvInput (not a field)
          paramInfo->drvInfo=strdup(drvInfo);
          dbFreeEntry(pdbentry);
          return asynSuccess;  // The correct record was found and the paramInfo structure is filled
        }
        else{
          //Not correct record. Do cleanup.
          if(paramInfo->isInput){
            free(paramInfo->inp);
            paramInfo->inp=0;
          }
          if(paramInfo->isOutput){
            free(paramInfo->out);
            paramInfo->out=0;
          }
          paramInfo->drvInfo=0;
          paramInfo->scan=0;
          paramInfo->dtyp=0;
          paramInfo->isInput=false;
          paramInfo->isOutput=false;
        }
      }
      status = dbNextRecord(pdbentry);
      free(paramInfo->recordName);
      paramInfo->recordName=0;
    }
    status = dbNextRecordType(pdbentry);
    free(paramInfo->recordType);
    paramInfo->recordType=0;
  }
  dbFreeEntry(pdbentry);
  return asynError;
}

int adsAsynPortDriver::getAmsPortFromDrvInfo(const char* drvInfo)
{
  //check if "ADSPORT" option in drvInfo string
  char plcAdr[MAX_FIELD_CHAR_LENGTH];
  int amsPortLocal=0;
  int nvals=sscanf(drvInfo,"ADSPORT=%d/%s",&amsPortLocal,plcAdr);
  if(nvals==2){
    return amsPortLocal;
  }
  return amsport_;  //No ADSPORT option found => return default port
}
/*
  ADSPORT=501/.ADR.16#5001,16#B,2,2=1;
*/

asynStatus adsAsynPortDriver::parsePlcInfofromDrvInfo(const char* drvInfo,adsParamInfo *paramInfo)
{
  bool err=false;
  //take part after last "/" if option or complete string.. How to handle .adr.??
  char plcAdrLocal[MAX_FIELD_CHAR_LENGTH];
  //See if option (find last '/')
  const char *drvInfoEnd=strrchr(drvInfo,'/');
  if(drvInfoEnd){ // found '/'
    int nvals=sscanf(drvInfoEnd,"/%s",plcAdrLocal);
    if(nvals==1){
      paramInfo->plcSymAdr=strdup(plcAdrLocal);
    }
    else{
      err=true;
    }
  }

  //Check if .ADR. command
  paramInfo->plcAbsAdrValid=false;
  paramInfo->plcSymAdrIsAdrCommand=false;
  const char *adrStr=strstr(drvInfo,ADR_COMMAND_PREFIX);
  if(adrStr){
    paramInfo->plcSymAdrIsAdrCommand=true;
    int nvals;
    nvals = sscanf(adrStr, ".ADR.16#%x,16#%x,%u,%u",
             &paramInfo->plcGroup,
             &paramInfo->plcOffsetInGroup,
             &paramInfo->plcSize,
             &paramInfo->plcDataType);

    if(nvals==4){
      paramInfo->plcAbsAdrValid=true;
    }
    else{
      err=true;
    }
  }
  //Look for AMSPORT option
  paramInfo->amsPort=getAmsPortFromDrvInfo(drvInfo);

  if(err){
    return asynError;
  }
  else{
    return asynSuccess;
  }
}

void adsAsynPortDriver::printParamInfo(adsParamInfo *paramInfo)
{
  printf("########################################\n");
  printf("  Record name:        %s\n",paramInfo->recordName);
  printf("  Record type:        %s\n",paramInfo->recordType);
  printf("  Record dataType:    %s\n",paramInfo->dtyp);
  printf("  Record asynType:    %d\n",paramInfo->asynType);
  printf("  Record scan:        %s\n",paramInfo->scan);
  printf("  Record inp:         %s\n",paramInfo->inp);
  printf("  Record out:         %s\n",paramInfo->out);
  printf("  Record isInput:     %d\n",paramInfo->isInput);
  printf("  Record isOutput:    %d\n",paramInfo->isOutput);
  printf("  Param index:        %d\n",paramInfo->paramIndex);
  printf("  Param drvInfo:      %s\n",paramInfo->drvInfo);
  printf("  Plc SymAdr:         %s\n",paramInfo->plcSymAdr);
  printf("  Plc Ams Port:       %d\n",paramInfo->amsPort);
  printf("  Plc SymAdrIsAdrCmd: %d\n",paramInfo->plcSymAdrIsAdrCommand);
  printf("  Plc AbsAdrValid:    %d\n",paramInfo->plcAbsAdrValid);
  printf("  Plc GroupNum:       16#%x\n",paramInfo->plcGroup);
  printf("  Plc OffsetInGroup:  16#%x\n",paramInfo->plcOffsetInGroup);
  printf("  Plc DataTypeSize:   %u\n",paramInfo->plcSize);
  printf("  Plc DataType:       %u\n",paramInfo->plcDataType);
  printf("########################################\n");
}

void adsAsynPortDriver::dbDumpRecords()
{
  DBENTRY *pdbentry;
  long status;

  pdbentry = dbAllocEntry(pdbbase);
  status = dbFirstRecordType(pdbentry);
  if(status) {printf("No record descriptions\n");return;}
  while(!status) {
    printf("record type: %s",dbGetRecordTypeName(pdbentry));
    status = dbFirstRecord(pdbentry);
    if(status) printf("  No Records\n");
      while(!status) {
       if(dbIsAlias(pdbentry))
         printf("\n  Alias:%s\n",dbGetRecordName(pdbentry));
       else {
         printf("\n  Record:%s\n",dbGetRecordName(pdbentry));
         status = dbFirstField(pdbentry,TRUE);
         if(status) printf("    No Fields\n");
           while(!status) {
             printf("    %s: %s",dbGetFieldName(pdbentry),
             dbGetString(pdbentry));
             status=dbNextField(pdbentry,TRUE);
           }
         }
         status = dbNextRecord(pdbentry);
      }
    status = dbNextRecordType(pdbentry);
  }
  printf("End of all Records\n");
  dbFreeEntry(pdbentry);
}

asynStatus adsAsynPortDriver::readOctet(asynUser *pasynUser, char *value, size_t maxChars,size_t *nActual, int *eomReason)
{
  const char* functionName = "readOctet";
  size_t thisRead = 0;
  int reason = 0;
  asynStatus status = asynSuccess;

  *value = '\0';
  lock();
  int error=CMDreadIt(value, maxChars);
  if (error) {
    status = asynError;
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:%s: error, CMDreadIt failed (0x%x).\n",
              driverName, functionName, error);
    unlock();
    return asynError;
  }

  thisRead = strlen(value);
  *nActual = thisRead;

  /* May be not enough space ? */
  if (thisRead > maxChars-1) {
    reason |= ASYN_EOM_CNT;
  }
  else{
    reason |= ASYN_EOM_EOS;
  }

  if (thisRead == 0 && pasynUser->timeout == 0){
    status = asynTimeout;
  }

  if (eomReason){
    *eomReason = reason;
  }

  *nActual = thisRead;
  asynPrint(pasynUser, ASYN_TRACE_FLOW,
             "%s thisRead=%lu data=\"%s\"\n",
             portName,
             (unsigned long)thisRead, value);
  unlock();
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:%s\n", driverName, functionName,value);

  return status;
}

asynStatus adsAsynPortDriver::writeOctet(asynUser *pasynUser, const char *value, size_t maxChars,size_t *nActual)
{

  const char* functionName = "writeOctet";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s: %s\n", driverName, functionName,value);

  size_t thisWrite = 0;
  asynStatus status = asynError;

  asynPrint(pasynUser, ASYN_TRACE_FLOW,
            "%s write.\n", /*ecmcController_p->*/portName);
  asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, value, maxChars,
              "%s write %lu\n",
              portName,
              (unsigned long)maxChars);
  *nActual = 0;

  if (maxChars == 0){
    return asynSuccess;
  }
  //lock();
  if (!(CMDwriteIt(value, maxChars))) {
    thisWrite = maxChars;
    *nActual = thisWrite;
    status = asynSuccess;
  }
  //unlock();
  asynPrint(pasynUser, ASYN_TRACE_FLOW,
            "%s wrote %lu return %s.\n",
            portName,
            (unsigned long)*nActual,
            pasynManager->strStatus(status));
  return status;
}

asynStatus adsAsynPortDriver::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
  const char* functionName = "readFloat64";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:\n", driverName, functionName);

  int function = pasynUser->reason;
  const char *paramName;
  getParamName(function, &paramName);
  *value=101.01999;

  asynPrint(pasynUser, ASYN_TRACE_INFO, "readFloat64: %s, %d\n",paramName,function);
  asynStatus status = asynSuccess;
  return status;

}

asynStatus adsAsynPortDriver::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  const char* functionName = "writeInt32";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:\n", driverName, functionName);

  asynStatus status = asynSuccess;
  return status;
}

asynStatus adsAsynPortDriver::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
  const char* functionName = "writeFloat64";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:\n", driverName, functionName);

  asynStatus status = asynSuccess;
  return status;
}

asynUser *adsAsynPortDriver::getTraceAsynUser()
{
  return pasynUserSelf;
}

asynStatus adsAsynPortDriver::readInt8Array(asynUser *pasynUser, epicsInt8 *value,size_t nElements, size_t *nIn)
{
  const char* functionName = "readInt8Array";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:\n", driverName, functionName);

  asynStatus status = asynSuccess;
  return status;
}

asynStatus adsAsynPortDriver::readInt16Array(asynUser *pasynUser, epicsInt16 *value,size_t nElements, size_t *nIn)
{
  const char* functionName = "readInt16Array";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:\n", driverName, functionName);

  asynStatus status = asynSuccess;
  return status;
}

asynStatus adsAsynPortDriver::readInt32Array(asynUser *pasynUser, epicsInt32 *value,size_t nElements, size_t *nIn)
{
  const char* functionName = "readInt32Array";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:\n", driverName, functionName);

  asynStatus status = asynSuccess;
  return status;
}

asynStatus adsAsynPortDriver::readFloat32Array(asynUser *pasynUser, epicsFloat32 *value,size_t nElements, size_t *nIn)
{
  const char* functionName = "readFloat32Array";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:\n", driverName, functionName);

  asynStatus status = asynSuccess;
  return status;
}

asynStatus adsAsynPortDriver::readFloat64Array(asynUser *pasynUser, epicsFloat64 *value,size_t nElements, size_t *nIn)
{
  const char* functionName = "readFloat64Array";
  asynPrint(pasynUser, ASYN_TRACE_INFO, "%s:%s:\n", driverName, functionName);

  asynStatus status = asynSuccess;
  return status;
}

asynStatus adsAsynPortDriver::setCfgData(const char *portName,
                                         const char *ipaddr,
                                         const char *amsaddr,
                                         unsigned int amsport,
                                         unsigned int priority,
                                         int noAutoConnect,
                                         int noProcessEos)
{
  portName_=portName;
  ipaddr_=ipaddr;
  amsaddr_=amsaddr;
  amsport_=amsport;
  priority_=priority;
  noAutoConnect_=noAutoConnect;
  noProcessEos_=noProcessEos;
  return asynSuccess;
}

asynStatus adsAsynPortDriver::addNotificationCallback(long port, const AmsAddr& server,adsParamInfo *paramInfo)
{
  const char* functionName = "addNotificationCallbackAbsAdr";

  const AdsNotificationAttrib attrib = {
      1,
      ADSTRANS_SERVERCYCLE,
      0,
      {4000000}
  };

  uint32_t hNotify=0;

  // Absolute access via group offset
  if(!paramInfo->plcSymAdrIsAdrCommand){

    if(!paramInfo->plcAbsAdrValid){
      asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: Absolute address in paramInfo not valid.\n", driverName, functionName);
      return asynError;
    }

    const long addStatus = AdsSyncAddDeviceNotificationReqEx(port,
                                                             &server,
                                                             paramInfo->plcGroup,
                                                             paramInfo->plcOffsetInGroup,
                                                             &attrib,
                                                             &adsNotifyCallback,
                                                             (uint32_t)paramInfo->paramIndex,
                                                             &hNotify);
  }
  else{ // Absolute access symbolic varaiable name

    paramInfo->hSymbolicHandle = getHandleByNameExample(out, port, server, paramInfo->plcSymAdr);
    
    uint32_t handle = 0;
    const long handleStatus = AdsSyncReadWriteReqEx2(port,
                                                     &server,
                                                     ADSIGRP_SYM_HNDBYNAME,
                                                     0,
                                                     sizeof(handle),
                                                     &handle,
                                                     paramInfo->plcSymAdr.size(),
                                                     paramInfo->plcSymAdr.c_str(),
                                                     nullptr);
    if (handleStatus) {
      asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: Create handle for %s failed with: 0x%x\n", driverName, functionName,paramInfo->plcSymAdr,handleStatus);
      return asynError;
    }

    //Add handle succeded
    paramInfo->hSymbolicHandle=handle;

    const long addStatus = AdsSyncAddDeviceNotificationReqEx(port,
                                                             &server,
                                                             ADSIGRP_SYM_VALBYHND,
                                                             paramInfo->hSymbolicHandle,
                                                             &attrib,
                                                             &NotifyCallback,
                                                             (uint32_t)paramInfo->paramIndex,
                                                             &hNotify);
  }

  if (addStatus){
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: Add device notification failed with: %ld\n", driverName, functionName,addStatus);
    return asynError;
  }

  //Add was successfull
  paramInfo->hCallbackNotify=hNotify;

  return asynSuccess;
}

asynStatus adsAsynPortDriver::delNotificationCallback(long port, const AmsAddr& server,adsParamInfo *paramInfo)
{
  const char* functionName = "delNotificationCallback";

  const long delStatus = AdsSyncDelDeviceNotificationReqEx(port, &server,paramInfo->hCallbackNotify);
  if (delStatus) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s:%s: Delete device notification failed with: %ld\n", driverName, functionName,delStatus);
    return asynError;
  }

  //Release symbolic handle if needed
  if(!paramInfo->plcSymAdrIsAdrCommand){
    releaseHandleExample(out, port, server, paramInfo->hSymbolicHandle);
  }
  return asynSuccess;
}

/* Configuration routine.  Called directly, or from the iocsh function below */

extern "C" {

  asynUser *pPrintOutAsynUser;

  static adsAsynPortDriver *adsAsynPortObj;
  /*
   * Configure and register
   */
  epicsShareFunc int
  adsAsynPortDriverConfigure(const char *portName,
                             const char *ipaddr,
                             const char *amsaddr,
                             unsigned int amsport,
                             unsigned int asynParamTableSize,
                             unsigned int priority,
                             int noAutoConnect,
                             int noProcessEos)
  {

    if (!portName) {
      printf("drvAsynAdsPortConfigure bad portName: %s\n",
             portName ? portName : "");
      return -1;
    }
    if (!ipaddr) {
      printf("drvAsynAdsPortConfigure bad ipaddr: %s\n",ipaddr ? ipaddr : "");
      return -1;
    }
    if (!amsaddr) {
      printf("drvAsynAdsPortConfigure bad amsaddr: %s\n",amsaddr ? amsaddr : "");
      return -1;
    }

    adsAsynPortObj=new adsAsynPortDriver(portName,ipaddr,amsaddr,amsport,asynParamTableSize,priority,noAutoConnect==0,noProcessEos);
    if(adsAsynPortObj){
      asynUser *traceUser= adsAsynPortObj->getTraceAsynUser();
      if(!traceUser){
        printf("adsAsynPortDriverConfigure: ERROR: Failed to retrieve asynUser for trace. \n");
        return (asynError);
      }
      pPrintOutAsynUser=traceUser;
      //adsAsynPortObj->connect(traceUser);
      //printf("ADSPORT IS %d\n",adsAsynPortObj->getAmsPortFromDrvInfo("ADSPORT=123/.ADR.16#5000,16#2,4,8"));
      //printf("ADSPORT IS %d\n",adsAsynPortObj->getAmsPortFromDrvInfo("ADSPORT=124/Main.M1.dTest"));
    }

    return asynSuccess;
  }

  /*
   * IOC shell command registration
   */
  static const iocshArg adsAsynPortDriverConfigureArg0 = { "port name",iocshArgString};
  static const iocshArg adsAsynPortDriverConfigureArg1 = { "ip-addr",iocshArgString};
  static const iocshArg adsAsynPortDriverConfigureArg2 = { "ams-addr",iocshArgString};
  static const iocshArg adsAsynPortDriverConfigureArg3 = { "default-ams-port",iocshArgInt};
  static const iocshArg adsAsynPortDriverConfigureArg4 = { "asyn param table size",iocshArgInt};
  static const iocshArg adsAsynPortDriverConfigureArg5 = { "priority",iocshArgInt};
  static const iocshArg adsAsynPortDriverConfigureArg6 = { "disable auto-connect",iocshArgInt};
  static const iocshArg adsAsynPortDriverConfigureArg7 = { "noProcessEos",iocshArgInt};
  static const iocshArg *adsAsynPortDriverConfigureArgs[] = {
    &adsAsynPortDriverConfigureArg0, &adsAsynPortDriverConfigureArg1,
    &adsAsynPortDriverConfigureArg2, &adsAsynPortDriverConfigureArg3,
    &adsAsynPortDriverConfigureArg4, &adsAsynPortDriverConfigureArg5,
    &adsAsynPortDriverConfigureArg6, &adsAsynPortDriverConfigureArg7};

  static const iocshFuncDef adsAsynPortDriverConfigureFuncDef =
    {"adsAsynPortDriverConfigure",8,adsAsynPortDriverConfigureArgs};

  static void adsAsynPortDriverConfigureCallFunc(const iocshArgBuf *args)
  {
    adsAsynPortDriverConfigure(args[0].sval,args[1].sval,args[2].sval,args[3].ival, args[4].ival, args[5].ival,args[6].ival,args[7].ival);
  }

  /*
   * This routine is called before multitasking has started, so there's
   * no race condition in the test/set of firstTime.
   */

  static void adsAsynPortDriverRegister(void)
  {
    iocshRegister(&adsAsynPortDriverConfigureFuncDef,adsAsynPortDriverConfigureCallFunc);
  }

  epicsExportRegistrar(adsAsynPortDriverRegister);
}



