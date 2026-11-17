// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fieldspec.h"

#include <KLocalizedString>

#include <QTest>

using namespace Qt::StringLiterals;

class FieldSpecTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void parsesThePlainForms();
    void comboTakesItsChoicesFromTheDefault();
    void comboMarksThePreselectedChoiceWithAStar();
    void intClampsItsDefaultIntoTheRange();
    void intKeepsANegativeLowerBound();
    void checkReadsTheUsualSpellingsOfYes();
    void colonsPastTheFifthFieldStayInTheExtra();
    void rejectsWhatItCannotUse();
};

void FieldSpecTest::initTestCase()
{
    // Without a domain every i18n() call in the error paths logs a warning of its own.
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kurodlg"));
}

void FieldSpecTest::parsesThePlainForms()
{
    QString error;
    const FieldSpec field = FieldSpec::parse(u"name:text:Your name:Pascal"_s, &error);

    QCOMPARE(field.type, FieldType::Text);
    QCOMPARE(field.name, u"name"_s);
    QCOMPARE(field.label, u"Your name"_s);
    QCOMPARE(field.defaultValue, u"Pascal"_s);
    QVERIFY(error.isEmpty());
}

void FieldSpecTest::comboTakesItsChoicesFromTheDefault()
{
    QString error;
    const FieldSpec field = FieldSpec::parse(u"preset:combo:Preset:p4|p5|p6"_s, &error);

    QCOMPARE(field.type, FieldType::Combo);
    QCOMPARE(field.choices, QStringList({u"p4"_s, u"p5"_s, u"p6"_s}));
    QCOMPARE(field.defaultValue, u"p4"_s);
}

void FieldSpecTest::comboMarksThePreselectedChoiceWithAStar()
{
    QString error;
    const FieldSpec field = FieldSpec::parse(u"preset:combo:Preset:p4|*p5|p6"_s, &error);

    QCOMPARE(field.choices, QStringList({u"p4"_s, u"p5"_s, u"p6"_s}));
    QCOMPARE(field.defaultValue, u"p5"_s);
}

void FieldSpecTest::intClampsItsDefaultIntoTheRange()
{
    QString error;
    const FieldSpec inRange = FieldSpec::parse(u"cq:int:CQ:28:10-51"_s, &error);

    QCOMPARE(inRange.minimum, 10);
    QCOMPARE(inRange.maximum, 51);
    QCOMPARE(inRange.number, 28);

    const FieldSpec tooHigh = FieldSpec::parse(u"cq:int:CQ:99:10-51"_s, &error);
    QCOMPARE(tooHigh.number, 51);

    const FieldSpec noDefault = FieldSpec::parse(u"cq:int:CQ::10-51"_s, &error);
    QCOMPARE(noDefault.number, 10);
}

void FieldSpecTest::intKeepsANegativeLowerBound()
{
    QString error;
    const FieldSpec field = FieldSpec::parse(u"offset:int:Offset:0:-10-10"_s, &error);

    QCOMPARE(field.type, FieldType::Int);
    QCOMPARE(field.minimum, -10);
    QCOMPARE(field.maximum, 10);
}

void FieldSpecTest::checkReadsTheUsualSpellingsOfYes()
{
    QString error;

    QVERIFY(FieldSpec::parse(u"t:check:10-Bit:on"_s, &error).checked);
    QVERIFY(FieldSpec::parse(u"t:check:10-Bit:true"_s, &error).checked);
    QVERIFY(FieldSpec::parse(u"t:check:10-Bit:1"_s, &error).checked);
    QVERIFY(!FieldSpec::parse(u"t:check:10-Bit:off"_s, &error).checked);
    QVERIFY(!FieldSpec::parse(u"t:check:10-Bit"_s, &error).checked);
}

void FieldSpecTest::colonsPastTheFifthFieldStayInTheExtra()
{
    QString error;
    const FieldSpec field = FieldSpec::parse(u"out:dir:Target:/mnt/a:b:c"_s, &error);

    QCOMPARE(field.type, FieldType::Dir);
    QCOMPARE(field.defaultValue, u"/mnt/a"_s);
    QVERIFY(error.isEmpty());
}

void FieldSpecTest::rejectsWhatItCannotUse()
{
    QString error;

    QCOMPARE(FieldSpec::parse(u"name:text"_s, &error).type, FieldType::Invalid);
    QVERIFY(!error.isEmpty());

    error.clear();
    QCOMPARE(FieldSpec::parse(u"name:wat:Label"_s, &error).type, FieldType::Invalid);
    QVERIFY(error.contains(u"wat"_s));

    error.clear();
    QCOMPARE(FieldSpec::parse(u":text:Label"_s, &error).type, FieldType::Invalid);
    QVERIFY(!error.isEmpty());

    error.clear();
    QCOMPARE(FieldSpec::parse(u"p:combo:Preset:"_s, &error).type, FieldType::Invalid);
    QVERIFY(!error.isEmpty());

    error.clear();
    QCOMPARE(FieldSpec::parse(u"cq:int:CQ:28:ten-to-fifty"_s, &error).type, FieldType::Invalid);
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(FieldSpecTest)

#include "fieldspectest.moc"
