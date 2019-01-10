#include <Ispitania/Ispitania.h>
#include <Dannie.h>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QDir>
#include <algorithm>
#include <vector>
#include "algorithm"

#include <Project/Project.h>
#include <KPI/KPI.h>
#include <Downloader/DownloaderGui.h>
#include <Tool/MessText/mess_text.h>


//ЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖ
//	Автоматическая проверка файлов КПИ, выдача true, если все переменные КПИ совпадают, false, если хотя бы одна не совпала
//ЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖЖ

bool Ispitania::autoCheckKpis(const PO* poFpoBvu, const QString & verBpoIspitania, const PO* poFpoBuk, const QString & verBpoIspitaniaBuk)
{
    Flag::TipiObjectov ObjectTip = Project::getTipObjectFlag(Rej->Object);
    if (ObjectTip!= Flag::Sputnic)
        return true;
    QString object=Rej->Object;
    int counterAdrKpi=0;
    QStringList errorKpis;//список КПИ с ошибками
    QStringList errorKpisHtml;
    QStringList errorVars;
    QString LogErrors;
    bool flagIgnoreNotFoundKpiTxt=false;
    QList <KPI> kpiAdrSpec, kpiAdr;
    // Проверка на наличие адресных КПИ
    for (auto item: Rej->kk)
    {
        KPI kpi(item->Naim, item->Id, item->IdSzk);
        if (kpi.IsAddress()&&(!kpi.IsDiskreditive()))
        {
            ++counterAdrKpi;
            if (kpi.name.contains("1363"))
                kpiAdrSpec.push_back(kpi);
            else
                kpiAdr.push_back(kpi);
        }
    }

    if (counterAdrKpi!=0)
    {
        QString MapOzuUrl=poFpoBvu->UrlMapOzu;

        // создание нужных подпапок
        QString pathToAll=QDir::tempPath()+"/SAI";
        QDir::temp().mkdir("SAI");
        QDir dirFiles(pathToAll);
        dirFiles.mkdir("VersKpi");
        dirFiles.mkdir("VersIsp");
        dirFiles.mkdir("Kpis");
        QDir dVersKpi(pathToAll+"/VersKpi/");
        QDir dVersIsp(pathToAll+"/VersIsp/");
        dVersIsp.mkdir("BUK");
        QDir dVersIspBuk(pathToAll+"/VersIsp/BUK/");
        QDir dKpi(pathToAll+"/Kpis/");
        errorKpisHtml = autoCheckKpiListForVers(*poFpoBvu, kpiAdr, dVersKpi, dVersIsp,dKpi, flagIgnoreNotFoundKpiTxt,verBpoIspitania);
        errorKpisHtml += autoCheckKpiListForVers(poFpoBuk, kpiAdrSpec, dVersKpi, dVersIspBuk,dKpi, flagIgnoreNotFoundKpiTxt,verBpoIspitaniaBuk);
    }
    QString errorKpisStr;
    QString errorKpisStrHtml;
    if (errorKpis.size()>0||errorKpisHtml.size()>0)
    {
        errorKpisStrHtml="<HTML> "
                         "<H3>Некоторые переменные для нижеперечисленных КПИ не совпадают с версией ФПО, используемой в испытании:" + verBpoIspitania+"</H3>"
                         " <BASEFONT SIZE =\"3\"> <font face=\"Times New Roman, Arial\">    <CENTER> "
                         "<TABLE BORDER=1 cellspacing=0 cellpadding=4 width=100%>"
                        "<COLGROUP align=\"center\">"
                        "<COLGROUP align=\"center\">"
                        "<COLGROUP align=\"center\">"
                        "<COLGROUP align=\"left\">";
        errorKpisStrHtml+="<TR BGCOLOR=YELLOW> <TH>КПИ</TH> <TH>Старая версия</TH> <TH> Новая версия</TH><TH> Комментарий</TH></TR>";

        ///  Добавляются строчки

        for (auto item: errorKpis)
            errorKpisStr+=item;

        for (auto item: errorKpisHtml)
            errorKpisStrHtml+=item;
        errorKpisStrHtml+=" </ТАВLЕ> < /CENTER><p><H4>Данные КПИ были дискредитированы. Более они не рекомендуются к использованию в создании нового моделируемого режима. </H4> <p> </HTML>";
        MessText* mes= new MessText(qApp->activeWindow());

        mes->configure(errorKpisStrHtml);

        mes->exec();
        bool Continue=mes->continueYes;
        delete mes;

        if (!Continue)
            return false;
    }

    return true;
};


QStringList Ispitania::autoCheckKpiListForVers(const PO*  currPo, QList<KPI> perKpi, QDir & dVersKpi, QDir & dVersIsp, QDir & dKpi, bool &flagIgnoreNotFoundKpiTxt, const QString & verBpoIspitania)
{
    QString LogErrors;
    QStringList errorKpisHtml;
        for (auto kpi:perKpi)
        {
            QString actualVersBpoForKpis=kpi.GelActualVersBpoForKpi(currPo->NaimPO, currPo->Object);
            //создание ПО актуальной версии для КПИ
            PO actualPoForKpi;
            actualPoForKpi.NaimPO=currPo->NaimPO;

            actualPoForKpi.Vers=actualVersBpoForKpis;
            actualPoForKpi.Object=currPo->Object;
            if (!actualPoForKpi.getUrlToMapOzu())
            {
                LogErrors+="Не найдена ссылка на MapOzu в ПО Перечень для версии:"+ actualPoForKpi.NaimPO + " " + actualPoForKpi.Vers + "\n";
                continue;
            }

            dVersKpi.mkdir(actualPoForKpi.Vers);

            // загружаем фалйы MapOzu для версии испытаний и актуальной версии КПИ
            if (!QFile(dVersKpi.path()+"/" + actualPoForKpi.Vers + "/MapOzu.txt").exists())
                actualPoForKpi.downloaderMapOzu(dVersKpi.path()+"/" + actualPoForKpi.Vers + '/', false);
            if (!QFile(dVersIsp.path()+"/" +"/MapOzu.txt").exists())
                    currPo->downloaderMapOzu(dVersIsp.path()+"/", false);

            // загрузка файла КПИ
            if (!kpi.LoadKpi(dKpi.path(), false, &flagIgnoreNotFoundKpiTxt))
                continue;

            QString NameForKpi=kpi.name.split(".kpi")[0]+".txt";//поскольку изначально имена КК заканчиваются .kpi

            //создаем 2 объекта КПИ. 1 для распарсивания по актуальному ПО для КПИ. 2ой для распарсивания по версии ПО испытаний
            KPI kpiActual(kpi.name, kpi.id ,kpi.idSzk);
            QString pathToKpi=dKpi.path()+"/kpi/"+kpi.idSzk+"/"+NameForKpi;
            kpiActual.parseKpiFile(pathToKpi);
            KPI kpiIsp(kpi.name, kpi.id ,kpi.idSzk);
            kpiIsp=kpiActual;

            kpiActual.ParseMapOzu(dVersKpi.path()+"/"+actualPoForKpi.Vers+"/MapOzu.txt");
            kpiIsp.ParseMapOzu(dVersIsp.path()+"/MapOzu.txt");


            if (!(kpiActual==kpiIsp))
            {
                //kpiActual.makeDiskredetive();
                KPI temp(kpi.name, kpi.id ,kpi.idSzk);
                QStringList TableHtmlKpi;
                TableHtmlKpi<<"<TR BGCOLOR=#A6FFA0> <TD>"+kpiActual.name+ "</TD> <TD> Версия: "+ actualVersBpoForKpis+"</TD><TD> Версия:"+ verBpoIspitania + "</TD><TD> Заменить на:</TD></TR>";
                KPI kpiAutoActualize(kpi.name, kpi.id ,kpi.idSzk);
                kpiAutoActualize.actualize(kpiActual, dVersIsp.path()+"/MapOzu.txt");
                TableHtmlKpi<<temp.diferentCCHtml(kpiActual, kpiIsp, kpiAutoActualize);
                errorKpisHtml<<TableHtmlKpi;
                kpiActual.makeDiskredetiveWithCommentHtml(TableHtmlKpi);
            }
        }
        return errorKpisHtml;
}
