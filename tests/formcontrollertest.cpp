// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#include "formcontroller.h"

#include <KLocalizedString>

#include <QDate>
#include <QTest>

using namespace Qt::StringLiterals;

class FormControllerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void startsWithTheDefaults();
    void aComboWithoutAMatchingDefaultOpensOnItsFirstChoice();
    void aNumberStaysInsideItsRange();
    void aChoiceOrDateThatDoesNotExistLeavesTheFieldAlone();
    void pickingAPresetSetsTheOtherFields();
    void changingAFieldLeavesThePresetItNoLongerMatches();
    void theDefaultsWinOverThePreselectedChoice();
    void printsNameValueLines();

private:
    static QList<FieldSpec> fields();
    static QList<PresetSpec> presets();
    static QList<PresetSpec> resolved(QList<PresetSpec> presets, const QList<FieldSpec>& fields);
};

void FormControllerTest::initTestCase()
{
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kurodlg"));
}

QList<FieldSpec> FormControllerTest::fields()
{
    QString error;

    return {
        FieldSpec::parse(u"preset:combo:Preset:Film|Camera|Custom"_s, &error),
        FieldSpec::parse(u"cq:int:Quality:28:1-51"_s, &error),
        FieldSpec::parse(u"tenbit:check:10 bit:off"_s, &error),
        FieldSpec::parse(u"when:date:When:2026-03-04"_s, &error),
        FieldSpec::parse(u"name:text:Name:Pascal"_s, &error),
    };
}

QList<PresetSpec> FormControllerTest::presets()
{
    QString error;

    return resolved(
        {
            PresetSpec::parse(u"preset:Film:cq=28:tenbit=off"_s, &error),
            PresetSpec::parse(u"preset:Camera:cq=20:tenbit=on"_s, &error),
        },
        fields());
}

QList<PresetSpec> FormControllerTest::resolved(QList<PresetSpec> presets, const QList<FieldSpec>& fields)
{
    QString error;
    resolvePresets(&presets, fields, &error);

    return presets;
}

void FormControllerTest::startsWithTheDefaults()
{
    const FormController controller(u"text"_s, fields());

    QCOMPARE(controller.values(), (QStringList{u"Film"_s, u"28"_s, u"off"_s, u"2026-03-04"_s, u"Pascal"_s}));
}

void FormControllerTest::aComboWithoutAMatchingDefaultOpensOnItsFirstChoice()
{
    // The parser never produces this, but a spec built by hand can.
    QList<FieldSpec> list = fields();
    list[0].defaultValue = u"Nonsense"_s;

    const FormController controller(QString(), list);

    QCOMPARE(controller.values().first(), u"Film"_s);
}

void FormControllerTest::aNumberStaysInsideItsRange()
{
    FormController controller(QString(), fields());

    controller.setValue(1, u"99"_s);
    QCOMPARE(controller.values().at(1), u"51"_s);

    controller.setValue(1, u"-4"_s);
    QCOMPARE(controller.values().at(1), u"1"_s);
}

void FormControllerTest::aChoiceOrDateThatDoesNotExistLeavesTheFieldAlone()
{
    FormController controller(QString(), fields());

    controller.setValue(0, u"Elsewhere"_s);
    QCOMPARE(controller.values().at(0), u"Film"_s);

    controller.setValue(3, u"not a date"_s);
    QCOMPARE(controller.values().at(3), u"2026-03-04"_s);
}

void FormControllerTest::pickingAPresetSetsTheOtherFields()
{
    FormController controller(QString(), fields(), presets());

    controller.setValue(0, u"Camera"_s);

    QCOMPARE(controller.values().at(1), u"20"_s);
    QCOMPARE(controller.values().at(2), u"on"_s);
}

void FormControllerTest::changingAFieldLeavesThePresetItNoLongerMatches()
{
    FormController controller(QString(), fields(), presets());

    controller.setValue(0, u"Camera"_s);

    // Quality no longer matches Camera, and no other preset holds either: the combo falls back to the
    // choice without a preset, and stays there even when the values happen to match Film again.
    controller.setValue(1, u"28"_s);
    QCOMPARE(controller.values().at(0), u"Custom"_s);

    controller.setValue(2, u"off"_s);
    QCOMPARE(controller.values().at(0), u"Custom"_s);
}

void FormControllerTest::theDefaultsWinOverThePreselectedChoice()
{
    // The combo says Film, the numbers say Camera: the form opens on what the numbers amount to.
    QString error;
    QList<FieldSpec> list = fields();
    list[1] = FieldSpec::parse(u"cq:int:Quality:20:1-51"_s, &error);
    list[2] = FieldSpec::parse(u"tenbit:check:10 bit:on"_s, &error);

    const FormController controller(QString(), list, resolved(presets(), list));

    QCOMPARE(controller.values().first(), u"Camera"_s);
}

void FormControllerTest::printsNameValueLines()
{
    const FormController controller(QString(), fields());

    QCOMPARE(controller.lines().at(1), u"cq=28"_s);
    QCOMPARE(controller.lines().last(), u"name=Pascal"_s);
}

QTEST_GUILESS_MAIN(FormControllerTest)
#include "formcontrollertest.moc"
