#include "KPI.h"

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QtAlgorithms>
#include <QDir>

#include <Dannie.h>
#include <FTP/FTP.h>



kpiVar::kpiVar(QString Address)
{
    address=Address;
}

KPI::KPI(QString Name)
{
    name=Name;
}

KPI::KPI (QString Name, QString Id, QString IdSzk)
{
    name=Name;
    id=Id;
    idSzk=IdSzk;
}

//************************************************************************************************************************
//  Проверка КПИ является ли она адресной(проверка проводится только по имени)
//************************************************************************************************************************

bool KPI::IsAddress()
{
    if (name.contains("1206")||name.contains("1353")||name.contains("1354")
            ||name.contains("1355")||name.contains("1356")||name.contains("1357")
            ||name.contains("1358")||name.contains("1359")||name.contains("1361"))
    {
        return true;
    }
    else
    {
        return false;
    }
}

//************************************************************************************************************************
//  Получение id актуальной версии БПО по KPI.idSzk, KPI.name, для объекта Object, с названием ПО NamePo.
//************************************************************************************************************************

QString KPI::GelActualVersBpoForKpi(QString NamePo, QString Object)
{
    QString timeKpi;
    QString idVers;
    QSqlQuery query501(QSqlDatabase::database(Base[1]));

    // получение времени создания КПИ
    query501.prepare("SELECT "
                     "kpis.claims_id, "
                     "kpis.name, "
                     "kpis.file_time "
                     "FROM "
                     "kpi.kpis "
                     "WHERE "
                     "claims_id = \'" + idSzk + "\' AND name= \'" + name + "\'");

    if (!query501.exec())
    {
        Mess(3, "Ошибка", "Ошибка при загрузке КПИ с именем:" + name + "\n" + query501.lastError().text());
        return "";
    }


    if(query501.size() == 0)
    {
        Mess(3, "Ошибка!!!", "В БД Base501 не найдена данная КПИ:" + name);
        return "";
    }

    while(query501.next())
    {
        timeKpi=query501.value(2).toString();
    }

    // получение шаблона для имени версии БПО
    QString shablon;
    QSqlQuery querySAI(QSqlDatabase::database(Base[0]));

    querySAI.prepare("SELECT "
                     "\"ПО Перечень\".\"Название ПО\", "
                     "\"ПО Перечень\".\"Объект\", "
                     "\"ПО Перечень\".\"Шаблон\" "
                     "FROM "
                     "public.\"ПО Перечень\" "
                     "WHERE "
                     "\"Название ПО\" like \'%" + NamePo + "%\' AND \"Объект\" = \'" + Object + "\'");

    if (!querySAI.exec())
    {
        Mess(3, "Ошибка", "Ошибка при загрузки шаблона :\n" +NamePo + " объекта:" + Object + "\n" + querySAI.lastError().text());
        return "";
    }

    if(querySAI.size() == 0)
    {
        Mess(3, "Ошибка!!!", "В перечне ПО не найден шаблон по указанным данным:");
        return "";
    }

    while(querySAI.next())
    {
        shablon=querySAI.value(2).toString();
    }

    //получение версии по шаблону имени и времени КПИ
    QString nameVers;
    QSqlQuery queryDbPm(QSqlDatabase::database(Base[2]));
    queryDbPm.prepare("SELECT "
                      "\"TableBuild\".\"ID\", "
                      "\"TableBuild\".\"VER_BPO\", "
                      "\"TableBuild\".\"DATE_UPLOAD\" "
                      "FROM "
                      "public.\"TableBuild\" "
                      "WHERE "
                      "\"DATE_BUILD\" = (SELECT "
                      "MAX(\"DATE_BUILD\") "
                      "FROM "
                      " public.\"TableBuild\" "
                      "WHERE "
                      "\"VER_BPO\" like \'" + shablon + "\' "
                                                        "AND "
                                                        " \"DATE_BUILD\" < \'" + timeKpi + "\' )");

    if (!queryDbPm.exec())
    {
        SendLog("QString KPI::GelActualVersBpoForKpi(QString NamePo, QString Object)","Ошибка при получении актуальной версии для КПИ:\n" + queryDbPm.lastError().text());
        return "";
    }

    if(queryDbPm.size() == 0)
    {
        SendLog("QString KPI::GelActualVersBpoForKpi(QString NamePo, QString Object)", "Ошибка при получении актуальной версии КПИ: не найдено шаблона Версии по имени " + shablon + " для времени, меньшего чем:" + timeKpi+ "(время КПИ:" + name + ").");
        return "";
    }

    while(queryDbPm.next())
    {
        idVers=queryDbPm.value(0).toString();
        nameVers=queryDbPm.value(1).toString();
    }

    qDebug()<<"KPI:"<<name<<" idVers:"<<idVers<<" nameVers:"<<nameVers;
    return nameVers;
}

//************************************************************************************************************************
//  Обработка текстового файла КПИ по пути path, с получением на выходе списка кодовых команд (со списком адресов, хранящихся в CC.KpiVars)
//************************************************************************************************************************

QList <CC> KPI::parseKpiFile(QString path)
{
    QList<CC> listCcs;
    CC codeCommand;
    QFile FileKpi(path);

    if (!FileKpi.exists())
        qDebug()<<"File don't exsists";
    if (FileKpi.open(QIODevice::ReadOnly| QIODevice::Text))
        qDebug()<<"File open to read";

    QString BegStr="├───┼─────┼─────────────────────────────────────────────────────────┼──────┼─────────────────────┤";
    QString EndStr="└───┴─────┴─────────────────────────────────────────────────────────┴──────┴─────────────────────┘";
    QString preLine;
    QTextStream in(&FileKpi);
    QString line;
    QStringList str;

    while (!in.atEnd())
    {
        line = in.readLine();

        //  Выход после считывания таблицы
        if(preLine == EndStr&&line.isEmpty())
            break;

        //  Поиск строки с номером команды
        str = line.split("│");
        preLine=line;

        if ((str.size()>5)&&((str[1]!="   ")&&(str[1]!=" № ")&&(str[1]!="п/п")))
        {
            codeCommand.Num=str[2].simplified();
            preLine=codeCommand.GetAdrByCcInFile(in);
            listCcs.push_back(codeCommand);
            codeCommand.KpiVars.clear();
        }

    }
    for (auto item: listCcs)
        Kks.push_back(item);
    return listCcs;
}

//************************************************************************************************************************
//  Получение списка адресов для одной кодовой команды из файла и записывание их в CC.KpiVars. Возвращаемая строка- последняя обработанная.
//************************************************************************************************************************

QString CC::GetAdrByCcInFile(QTextStream & in)
{
    QString line;
    QStringList str;
    while (!in.atEnd())
    {
        //  чтение файла
        line = in.readLine();
        //  выделение КК
        str = line.split("│");

        //  Обработка ошибки файла или выход в конце блока
        if (! (str.size()>5)
                || (str[1]!="   "))
        {
            return line;
        }

        //  Обработка КК

        switch (Num.toInt())
        {
        case 1206:
            break;
        case 1353:
            parseStandartCc(str);
            break;
        case 1354:
            parseStandartCc(str);
            break;
        case 1355:
            GetAdr1355InLine(str);
            break;
        case 1356: //Изменить ОЗУ БУК
            if (str[2].contains("p3")&&(!str[2].contains("p30")))
                KpiVars.push_back(kpiVar (str[5].simplified()));
            break;
        case 1357:
            parseStandartCc(str);
            break;
        case 1358:
            parseStandartCc(str);
            break;
        case 1359:
        {
            QRegExp reg("p3$");
            if (str[2].contains(reg))
            {
                bool ok;
                str[5].simplified().toInt(& ok, 16);
                if (ok)
                {
                    uint pg=str[5].simplified().toUInt(& ok, 16);
                    pg=pg>>8;
                    page=pg;
                }
            };
            if (str[2].contains("p4"))
                KpiVars.push_back(kpiVar (str[5].simplified()));
        }
            break;
        case 1361:
            parseStandartCc(str);
            break;
        default:
            qDebug()<<"Not adress CC";
            break;
        }
    }
}

//************************************************************************************************************************
//  Обработка строки для простых КК (кроме 1355, 1356)
//************************************************************************************************************************

void CC::parseStandartCc(const QStringList & str)
{
    QRegExp reg("p2$");
    if (str[2].contains(reg))
    {
        bool ok;
        str[5].simplified().toInt(& ok, 16);
        if (ok)
        {
            uint pg=str[5].simplified().toUInt(& ok, 16);
            pg=pg>>8;
            page=pg;
        }
    };
    if (str[2].contains("p3")&&(!str[2].contains("p30")))
        KpiVars.push_back(kpiVar (str[5].simplified()));
}

//************************************************************************************************************************
//  Обработка строки для кодовой команды 1355
//************************************************************************************************************************

void CC::GetAdr1355InLine(const QStringList & str)
{
    QRegExp reg("p2$");
    if (str[2].contains(reg))
    {
        bool ok;
        str[5].simplified().toInt(& ok, 16);
        if (ok)
        {
            uint pg=str[5].simplified().toUInt(& ok, 16);
            pg=pg>>8;
            page=pg;
        }
    };
    if ((str[2].contains("p3") || str[2].contains("p5") || str[2].contains("p7")||
         str[2].contains("p9") || str[2].contains("p11") || str[2].contains("p13")||
         str[2].contains("p15") || str[2].contains("p17") || str[2].contains("p19")||
         str[2].contains("p21") || str[2].contains("p23") || str[2].contains("p25")||
         str[2].contains("p27") || str[2].contains("p29")) && (!str[2].contains("p30")))
    {
        KpiVars.push_back(kpiVar (str[5].simplified()));
    }
}

//************************************************************************************************************************
//  Вывод кодовой команды в консоль
//************************************************************************************************************************

void CC::print() const
{
    qDebug()<<"Code command:"<<Num;
    qDebug()<<"Page:"<<page;
    for (auto item:KpiVars)
    {
        qDebug()<<item.name<<":"<<item.address;
    }
}

//************************************************************************************************************************
//  Загрузка файла КПИ.txt по имени КПИ и idSzk(claims_id в базе Base501.kpi.kpis)
//  файлы загружаются по пути PutSave/kpi/idSzk КПИ/имя КПИ.txt
//************************************************************************************************************************

bool KPI::LoadKpi(QString PutSave, bool CreateDir, bool * flagIgnore)
{
    QStringList SpisKpi501Loc, SpisKpi501ftp;
    QSqlDatabase db = QSqlDatabase::database("SAI");
    QSqlQuery query(db);
    QString s, MestoZapisi, NaimTemp;
    QStringList mk, temp;
    if(PutSave != "")
    {
        if(CreateDir)
            MestoZapisi = PutSave + "/" + PerevodNaLatBukvi(name) + "/";
        else
            MestoZapisi = PutSave + "\\";
    }
    else
    {
        Mess(3, "Ошибочка", "отсутствует путь для сохранения данных");
        return false;
    }

    if(MestoZapisi.contains("\\"))
        temp = MestoZapisi.split("\\");
    else
        temp = MestoZapisi.split("/");

    s.clear();
    s = temp.first();
    temp.removeFirst();
    while(!temp.isEmpty())
    {
        s = s + "\\" + temp.first();
        mk << s;
        temp.removeFirst();
    }

    mk << MestoZapisi + "\\kpi";

    QSqlQuery query501(QSqlDatabase::database(Base[1]));
    query501.prepare("SELECT "
                     "claims.ship_id, kpis.kpis_id, "
                     "kpis.claims_id, "
                     "kpis.name, "
                     "'/kpi/'||kpis.claims_id||'/kpi/'||'claim_'||kpis.claims_id||'_file_'||kpis.kpis_id AS ftplink "
                     "FROM "
                     "kpi.kpis, "
                     "kpi.claims, "
                     "kpi.claim_states "
                     "WHERE "
                     "claims.claims_id = kpis.claims_id AND "
                     "claim_states.claim_states_id = claims.claim_states_id AND "
                     "claim_states.status = 5 AND "
                     "kpis.deleted = 0 AND "
                     "kpis.stored = 1 AND "
                     "(kpis.name LIKE '%.txt') "
                     "and kpis.name like :NaimKPI "
                     "and kpis.claims_id = :IdSzk "
                     "ORDER BY "
                     "claims.ship_id, kpis.name; ");
    NaimTemp = name;
    NaimTemp.replace(".kpi", ".%");
    query501.bindValue(":NaimKPI", NaimTemp);
    query501.bindValue(":IdSzk", idSzk);

    if (!query501.exec())
    {
        Mess(3, "Ошибка", "Ошибка при загрузки пути к КПИ\n" + query501.lastError().text());
        return false;
    }

    NaimTemp.replace(".%", ".txt");

    if(query501.size() == 0)
    {
        if (*flagIgnore==false)
        {
            bool *&IgnoreRef=flagIgnore;
            int mes_rez;
            mes_rez=Mess(3, "Ошибка!!!", "В БД Base501 не найден файл:" + NaimTemp,
                         "Продолжить", "Игнорировать отсутствие остальных файлов КПИ.txt");
            if (mes_rez==1)
            {
                *IgnoreRef=true;
                *flagIgnore=true;
            }
        }
        return false;
    }

    while(query501.next())
    {
        if (!QFile(MestoZapisi + "kpi\\" + query501.value(2).toString() + "\\" + query501.value(3).toString()).exists())
        {
            SpisKpi501Loc << MestoZapisi + "kpi\\" + query501.value(2).toString() + "\\" + query501.value(3).toString();
            SpisKpi501ftp << query501.value(4).toString();
            mk << MestoZapisi + "kpi\\" + query501.value(2).toString();
        }
        else
            qDebug()<<"KPI already exsists";
    }
    QDir d;
    while(!mk.isEmpty())
    {
        d.mkdir(mk.first());
        mk.removeFirst();
    }
    ClientFTP* cl = new ClientFTP(1, SpisKpi501ftp, SpisKpi501Loc);
    cl->exec();
    return true;
}

//************************************************************************************************************************
//  Получение имен переменных KpiVars по их ранее определенным адресам для каждой КК из списка Kks
//  (не обрабатывает переменные с адресом 0), путь указывается до файла MapOzu, а не до папки, его содержащей.
//************************************************************************************************************************

void KPI::ParseMapOzu(QString pathToMapOzu)
{
    QFile FileMapOzu(pathToMapOzu);
    if (!FileMapOzu.exists())
    {
        qDebug()<<"File don't exsists";
        return;
    }
    if (FileMapOzu.open(QIODevice::ReadOnly| QIODevice::Text))
    {
        qDebug()<<"File open to read";
    }

    QTextStream in(&FileMapOzu);
    QString line;
    QString lastPage="0";
    QStringList MapOzu;
    while(!in.atEnd())
    {
        line = in.readLine();
        MapOzu=line.split('|');
        if (MapOzu.size()>4)
        {
            if (MapOzu[1].contains(QRegExp("\\d")))//найден номер страницы
            {
                lastPage=MapOzu[1].simplified();
            }
            for (int i=0; i<Kks.size();++i)
                for (int j=0; j<Kks[i].KpiVars.size();++j)
                    if (( Kks[i].KpiVars[j].address != "0") && (line.contains(Kks[i].KpiVars[j].address.toLower())||line.contains(Kks[i].KpiVars[j].address)))
                        // поскольку в МапОЗУ адреса пишутся строчными буквами, а в файле КПИ заглавными
                    {
                        if ( lastPage.toUInt()==Kks[i].page)
                        {
                            Kks[i].KpiVars[j].name=MapOzu[4].simplified();
                        }
                    }
        }
    }

    QStringList missedVars;
    for (auto item: Kks)
    {
        for (auto var: item.KpiVars)
        {
            if (var.address!="0" && var.name=="")
                missedVars<<var.address;
        }
    }
    if (missedVars.size()>0)
    {
        qDebug()<<missedVars;
    }
}

//************************************************************************************************************************
//  Получение адресов переменных KpiVars по их ранее определенным именам для каждой КК из списка Kks
//  (не обрабатывает переменные с адресом 0), путь указывается до файла MapOzu, а не до папки, его содержащей.
//  Возварщает true при успешном выполнении, false при возникновении ошибок.
//************************************************************************************************************************

bool KPI::ParseMapOzuByNames(QString pathToMapOzu)
{
    int counterFind=0;
    QFile FileMapOzu(pathToMapOzu);
    if (!FileMapOzu.exists())
    {
        qDebug()<<"File don't exsists";
        return false;
    }
    if (FileMapOzu.open(QIODevice::ReadOnly| QIODevice::Text))
    {
        qDebug()<<"File open to read";
    }

    QTextStream in(&FileMapOzu);
    QString line;
    QString lastPage="0";
    QStringList MapOzu;
    while(!in.atEnd())
    {
        line = in.readLine();
        MapOzu=line.split('|');
        if (MapOzu.size()>4)
        {
            if (MapOzu[1].contains(QRegExp("\\d")))//найден номер страницы
            {
                lastPage=MapOzu[1].simplified();//сохраняем на какой странице мы находимся
            }
            for (int i=0; i<Kks.size();++i)
                for (int j=0; j<Kks[i].KpiVars.size();++j)
                {
                    if ( Kks[i].KpiVars[j].address != "" && Kks[i].KpiVars[j].name != "" && lastPage.toUInt()==Kks[i].page && line.contains(Kks[i].KpiVars[j].name))
                    {
                        Kks[i].KpiVars[j].address=MapOzu[3].simplified();
                        ++counterFind;
                    }
                }
        }
    }
    int counterAll=0;
    int counterNoNames=0;
    //Увеличение адреса для переменных без имени на 2 от предыдущего.
    for (int i=0; i<Kks.size();++i)
        for (int j=0; j<Kks[i].KpiVars.size();++j)
        {
            if (Kks[i].KpiVars[j].address!="0")
            {
                if ( Kks[i].KpiVars[j].name == ""&&(j>0))
                {
                    bool ok;
                    uint adr=Kks[i].KpiVars[j-1].address.toUInt(& ok, 16);
                    adr+=2;
                    if (ok)
                    {
                        QString temp=QString("0x%1").arg(adr, 0, 16);
                        Kks[i].KpiVars[j].address=temp;
                        ++counterNoNames;
                    }
                }
                ++counterAll;
            }
        }
    if (counterAll!=counterFind+counterNoNames)
    {
        qDebug()<<"Не удалось атоматически актуализировать данную КПИ";
        return false;
    }
    return true;
}

//************************************************************************************************************************
//  Перегрузка оператора сравнения для работы с пользовательским классом KPI сравнивает размеры списков KpiVars, их
//  адресса и имена (не сравнивает по именам и какой либо другой информации самой КПИ, кроме списка КК )
//************************************************************************************************************************

bool operator==(const KPI & lhs, const KPI & rhs)
{
    if (lhs.Kks.size()!=rhs.Kks.size())
        return false;
    for (int i=0; i<lhs.Kks.size();++i)
    {
        if (lhs.Kks[i].KpiVars.size()!=rhs.Kks[i].KpiVars.size())
            return false;
        for (int j=0;j<lhs.Kks[i].KpiVars.size(); ++j)
            if (lhs.Kks[i].KpiVars[j].address != rhs.Kks[i].KpiVars[j].address || lhs.Kks[i].KpiVars[j].name != rhs.Kks[i].KpiVars[j].name )
                return false;
    }
    return true;
}

//************************************************************************************************************************
//  Вывод состава KPI в консоль
//************************************************************************************************************************

void KPI::print()
{
    qDebug()<<name;
    qDebug()<<idSzk;
    qDebug()<<id;
    for (auto item:Kks)
        item.print();
}

//************************************************************************************************************************
//  Проверка КПИ на дискредитивность
//************************************************************************************************************************

bool KPI::IsDiskreditive()
{
    QSqlQuery query_diskr(QSqlDatabase::database("SAI"));
    QString diskr;
    diskr = "SELECT "
            "\"Идентификатор КПИ\" "
            "FROM "
            "\"КПИ дискредитированные\" "
            "Where \"Идентификатор КПИ\" = ?";

    query_diskr.prepare(diskr);
    query_diskr.addBindValue(id);
    if(!query_diskr.exec())
    {
        Mess(3, "Ошибочка вышла", "Проблема при загрузке данных КПИ.\nПодробнее:\n"
             + query_diskr.lastError().text());
        return false;
    }
    while(query_diskr.next())
    {
        return true;
    }
    return false;
}

//************************************************************************************************************************
//  Пометка КПИ как дискредитивной
//************************************************************************************************************************

void KPI::makeDiskredetive()
{
    QSqlQuery queryKpiFiks(QSqlDatabase::database("SAI"));
    QString strPomFiks = "INSERT INTO \"КПИ дискредитированные\" VALUES (DEFAULT, "+id+", DEFAULT, "+LichId+" )";
    if(!queryKpiFiks.exec(strPomFiks))
    {
        Mess(3, "Ошибочка вышла", "Проблема при загрузке данных КПИ.\nПодробнее:\n"
             + queryKpiFiks.lastError().text());
        return;
    }
}

//************************************************************************************************************************
//  Пометка КПИ как дискредитивной, с комменарием в формате ячеек таблицы
//************************************************************************************************************************

void KPI::makeDiskredetiveWithCommentHtml(QStringList comment)
{
    QString htmlTable;
    htmlTable="<HTML> "
              "<HEAD>"
                      "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">"
                      "</HEAD><BODY>"
                     " <BASEFONT SIZE =\"3\"> <font face=\"Times New Roman, Arial\">    <CENTER> "
                     "<TABLE BORDER=1 cellspacing=0 cellpadding=4 width=100%>"
                    "<COLGROUP align=\"center\">"
                    "<COLGROUP align=\"center\">"
                    "<COLGROUP align=\"center\">"
                    "<COLGROUP align=\"left\">";
    htmlTable+="<TR BGCOLOR=YELLOW> <TH>КПИ</TH> <TH>Старая версия</TH> <TH> Новая версия</TH><TH> Комментарий</TH></TR>";

    for (auto item: comment)
        htmlTable+=item;
    htmlTable+=" </ТАВLЕ> </BODY> </HTML>";
    QSqlQuery queryKpiFiks(QSqlDatabase::database("SAI"));
    QString strPomFiks = "INSERT INTO \"КПИ дискредитированные\" VALUES (DEFAULT, "+id+", DEFAULT, "+LichId+", \'" + htmlTable +"\' )";
    if(!queryKpiFiks.exec(strPomFiks))
    {
        Mess(3, "Ошибочка вышла", "Проблема при пометке КПИ дискредитивной с комменатрием.\nПодробнее:\n"
             + queryKpiFiks.lastError().text());
        return;
    }
}

//************************************************************************************************************************
//  Оператор присваивания для КПИ
//************************************************************************************************************************

KPI & KPI::operator=(const KPI & kpi)
{
    name=kpi.name;
    id=kpi.id;
    idSzk=kpi.idSzk;
    Kks=kpi.Kks;
    return *this;
}

//************************************************************************************************************************
//  Актуализация адресов переменных КПИ, если все имнеа совпали. Возвращает true, если все прошло успешно.
//************************************************************************************************************************

bool KPI::actualize(const KPI & kpiOld, QString pathToVers)
{
    name=kpiOld.name;
    id=kpiOld.id;
    idSzk=kpiOld.idSzk;
    Kks=kpiOld.Kks;
    return ParseMapOzuByNames(pathToVers);
}

//************************************************************************************************************************
//  Нахождение различий между КК КПИ. На выходе получаем строку об ошибке или список строк с переменными, у которых
//  различается адрес или имя.
//************************************************************************************************************************

QStringList KPI::diferentCC(const KPI& lhs, const KPI& rhs, const KPI& autoKpi)
{
    QStringList result;
    if (lhs.Kks.size()!=rhs.Kks.size())
        return result<<"Не совпадение количества КК\n";
    else
    for (int i=0; i<lhs.Kks.size();++i)
    {
        if (lhs.Kks[i].KpiVars.size()!=rhs.Kks[i].KpiVars.size())
            return result<<"Не совпадение количества переменных КК\n";
        else
        for (int j=0;j<lhs.Kks[i].KpiVars.size(); ++j)
            if (lhs.Kks[i].KpiVars[j].address != rhs.Kks[i].KpiVars[j].address || lhs.Kks[i].KpiVars[j].name != rhs.Kks[i].KpiVars[j].name )
                result<<lhs.Kks[i].KpiVars[j].address + "-" + lhs.Kks[i].KpiVars[j].name + " : " + rhs.Kks[i].KpiVars[j].address + "-" + rhs.Kks[i].KpiVars[j].name +"\n" +
                        "Предлагается заменить на:" + autoKpi.Kks[i].KpiVars[j].address + "-" + autoKpi.Kks[i].KpiVars[j].name + "\n";
    }
    result<<"\n";
    return result;
}

QStringList KPI::diferentCCHtml(const KPI& lhs, const KPI& rhs, const KPI& autoKpi)
{
    QStringList result;
    if (lhs.Kks.size()!=rhs.Kks.size())
        return result<<"<TR>Не совпадение количества КК</TR>";
    else
    for (int i=0; i<lhs.Kks.size();++i)
    {
        if (lhs.Kks[i].KpiVars.size() != rhs.Kks[i].KpiVars.size())
            return result<<"<TR>Не совпадение количества переменных КК</TR>";
        else
        for (int j=0;j<lhs.Kks[i].KpiVars.size(); ++j)
            if (lhs.Kks[i].KpiVars[j].address != rhs.Kks[i].KpiVars[j].address || lhs.Kks[i].KpiVars[j].name != rhs.Kks[i].KpiVars[j].name )
            {
                QString zamenenaiaKpi;
                if (lhs.Kks[i].KpiVars[j].address == autoKpi.Kks[i].KpiVars[j].address && lhs.Kks[i].KpiVars[j].name == autoKpi.Kks[i].KpiVars[j].name)
                    zamenenaiaKpi="<TD> Переменной " + autoKpi.Kks[i].KpiVars[j].name + "-нет</TD></TR>";
                else
                    zamenenaiaKpi="<TD>" + autoKpi.Kks[i].KpiVars[j].address + "   " + autoKpi.Kks[i].KpiVars[j].name + " </TD></TR>";
                result<<"<TR><TD></TD> <TD> "+lhs.Kks[i].KpiVars[j].address + "  " + lhs.Kks[i].KpiVars[j].name + " </TD>"
                        "<TD> " + rhs.Kks[i].KpiVars[j].address + "  " + rhs.Kks[i].KpiVars[j].name +" </TD> " + zamenenaiaKpi;
            }
    }
    return result;
}


