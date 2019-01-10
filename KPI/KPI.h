#ifndef KPI_H
#define KPI_H

#include <QObject>
#include <QFile>
#include <QDate>
#include <QTextStream>

class CC;
class kpiVar;

class KPI
{
public:
    QString name;
    QString id;
    QString idSzk;
    QList <CC> Kks;
    KPI(QString name);
    KPI (QString Name, QString Id, QString IdSzk);
    bool IsDiskreditive();
    void makeDiskredetive();
    void makeDiskredetiveWithCommentHtml(QStringList comment);
    void cancelDiskredetive();
    bool IsAddress();
    QString GelActualVersBpoForKpi(QString NameVers, QString Object);
    bool CheckDownloadMapOzuForBpoIspAndActual(int versIdBPOIsp,int versIdBPOActual );
    void downloaderMapOzu(QString PathToSave, QString url);
    bool LoadKpi(QString PutSave, bool CreateDir, bool * flagIgnore);
    void print();
    void ParseMapOzu(QString pathToMapOzu);
    bool ParseMapOzuByNames(QString pathToMapOzu);
    QList <CC> parseKpiFile(QString path);
    QVector<CC> replaseKpiForLastVersionBpo(const QVector <CC> & KpiOlds, QFile oldVers, QFile currentVers);// возвращает список замененных кодовых команд  KpiOlds (в основном меняется только адрес), посредством сравнения 2х файлов MapOzu старой и текущей версии)
    bool actualize(const KPI & kpiOld, QString pathToVers);
    KPI & operator=(const KPI & kpi);
    QStringList diferentCC(const KPI& lhs, const KPI& rhs, const KPI& autoKpi);
    QStringList diferentCCHtml(const KPI& lhs, const KPI& rhs, const KPI& autoKpi);
};

bool operator==(const KPI & lhs, const KPI & rhs);

class CC // (code command) все, кроме KpiVars и page- необязательные поля, которые зависят от //того, какая кодовая команда была подана.
{
public:
    QList<kpiVar> KpiVars;
    QString Num;// по перечню КК
    uint page;
    int countWord;
    QString GetAdrByCcInFile(QTextStream & file);
    void print() const;
private:
    void GetAdr1355InLine(const QStringList & str);
    void parseStandartCc(const QStringList & str);
};


class kpiVar
{
public:
    QString name;
    QString address;
    int page;
    kpiVar(QString Address);
    kpiVar(QString Name, QString Address, int Page);
};


#endif // KPI_H
