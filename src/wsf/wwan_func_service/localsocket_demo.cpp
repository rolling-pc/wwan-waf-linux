#include "wwan_func.hpp"
#include <QWaitCondition>
#include <QString>

#include "log.hpp"

using namespace wwan_func;
using namespace afal::log;

void GetFlashProgress(flashProgress &flashprogressmsg) {
    LOG_DEBUG("======================= Ind Start : Added ======================");
    LOG_DEBUG("flash indication Callback called:");
    LOG_DEBUG("progress:%s", QS(flashprogressmsg.bus));
    LOG_DEBUG("flashMessage:%s", QS(flashprogressmsg.flashMessage));
    LOG_DEBUG("======================= Flash Ind Stop========================");
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    Logger::Instance().Init(CONSOLE, "WwanFuncSvcLib Demo");
    Logger::Instance().EnableDebugMode(1);

    WwanFuncSvcClient wwansvcclient;
    flashProgress flashprogess;
    wwansvcclient.Init();
    wwansvcclient.SubscribeDeviceEvent(GetFlashProgress);
    
    bool flashresult;
    QString filepath = "c:/flash.xml";
    int erasemode = 0;
    int resetType = 0; // 0-default , 1- PLDR, 2-FLDR, 3-MBIM
    bool ret = wwansvcclient.TriggerFlash(flashresult,filePath,erasemode,resetType);

    LOG_DEBUG("======================= resp recived   =====================");
    LOG_DEBUG("TriggerFlash resp:");
    LOG_DEBUG("flashresult:%d", flashresult);
    
    bool setresult;
    bool ret = wwansvcclient.SvcAvailable(setresult);
    LOG_DEBUG("======================= resp recived   =====================");
    LOG_DEBUG("SvcAvailable resp:");
    LOG_DEBUG("Svc status:%d", setresult); // 0- no for flash, 1 - ok for trigger flash 

    bool ret = wwansvcclient.TurnoffSwitch(flashresult,filePath,erasemode,resetType);
    LOG_DEBUG("======================= resp recived   =====================");
    LOG_DEBUG("TurnoffSwitch resp:");
    LOG_DEBUG("Svc setting status:%d", setresult); // 1 - ok 

    bool ret = wwansvcclient.TurnonSwitch(flashresult,filePath,erasemode,resetType);
    LOG_DEBUG("======================= resp recived   =====================");
    LOG_DEBUG("TurnonSwitch resp:");
    LOG_DEBUG("Svc setting status:%d", setresult); // 1 - ok  
    
    app.exec();
    wwansvcclient.Deinit();

    return true;
}