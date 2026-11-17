// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presetspec.h"

#include <KLocalizedString>

#include <QHash>
#include <QTest>

using namespace Qt::StringLiterals;

class PresetSpecTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void parsesFieldChoiceAndAssignments();
    void rejectsWhatItCannotUse();
    void resolvingSpellsValuesTheWayTheFormPrintsThem();
    void resolvingRejectsWhatTheFieldsCannotTake();
    void matchKeepsThePickedChoiceWhileItHolds();
    void matchMovesToThePresetTheFieldsNowHold();
    void matchFallsBackToTheChoiceWithoutAPreset();
    void matchKeepsAChoiceWithoutAPreset();

private:
    static QList<FieldSpec> fields();
    static QList<PresetSpec> presets();
    static QString match(const QString& current, const QString& cq, const QString& tenBit);
};

void PresetSpecTest::initTestCase()
{
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kurodlg"));
}

QList<FieldSpec> PresetSpecTest::fields()
{
    QString error;

    return {
        FieldSpec::parse(u"preset:combo:Preset:Film|Camera|Archive|Custom"_s, &error),
        FieldSpec::parse(u"cq:int:Quality:28:1-51"_s, &error),
        FieldSpec::parse(u"tenbit:check:10 bit:off"_s, &error),
    };
}

QList<PresetSpec> PresetSpecTest::presets()
{
    QString error;
    QList<PresetSpec> presets = {
        PresetSpec::parse(u"preset:Film:cq=28:tenbit=off"_s, &error),
        PresetSpec::parse(u"preset:Camera:cq=20:tenbit=false"_s, &error),
        PresetSpec::parse(u"preset:Archive:cq=020:tenbit=1"_s, &error),
    };

    resolvePresets(&presets, fields(), &error);
    return presets;
}

/// What the combo shows with `current` picked and the other two fields holding these values.
QString PresetSpecTest::match(const QString& current, const QString& cq, const QString& tenBit)
{
    const QHash<QString, QString> form = {{u"cq"_s, cq}, {u"tenbit"_s, tenBit}};

    return matchingChoice(fields().first(), current, presets(), [&](const QString& name) { return form.value(name); });
}

void PresetSpecTest::parsesFieldChoiceAndAssignments()
{
    QString error;
    const PresetSpec preset = PresetSpec::parse(u"preset:DVD film:cq=28:out=a=b"_s, &error);

    QCOMPARE(preset.field, u"preset"_s);
    QCOMPARE(preset.choice, u"DVD film"_s);
    QCOMPARE(preset.values.size(), 2);
    QCOMPARE(preset.values.at(0), std::make_pair(u"cq"_s, u"28"_s));
    QCOMPARE(preset.values.at(1), std::make_pair(u"out"_s, u"a=b"_s));
    QVERIFY(error.isEmpty());
}

void PresetSpecTest::rejectsWhatItCannotUse()
{
    QString error;

    QVERIFY(PresetSpec::parse(u"preset:Film"_s, &error).field.isEmpty());
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(PresetSpec::parse(u":Film:cq=28"_s, &error).field.isEmpty());
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(PresetSpec::parse(u"preset:Film:cq"_s, &error).field.isEmpty());
    QVERIFY(error.contains(u"cq"_s));

    error.clear();
    QVERIFY(PresetSpec::parse(u"preset:Film:=28"_s, &error).field.isEmpty());
    QVERIFY(!error.isEmpty());
}

void PresetSpecTest::resolvingSpellsValuesTheWayTheFormPrintsThem()
{
    const QList<PresetSpec> resolved = presets();

    QCOMPARE(resolved.at(1).values.at(1).second, u"off"_s);
    QCOMPARE(resolved.at(2).values.at(0).second, u"20"_s);
    QCOMPARE(resolved.at(2).values.at(1).second, u"on"_s);
}

void PresetSpecTest::resolvingRejectsWhatTheFieldsCannotTake()
{
    const auto rejects = [](const QString& spec)
    {
        QString error;
        QList<PresetSpec> presets = {PresetSpec::parse(spec, &error)};
        return !resolvePresets(&presets, fields(), &error) && !error.isEmpty();
    };

    QVERIFY(rejects(u"nothing:Film:cq=28"_s));
    QVERIFY(rejects(u"cq:Film:tenbit=on"_s));
    QVERIFY(rejects(u"preset:Blu-ray:cq=28"_s));
    QVERIFY(rejects(u"preset:Film:crf=28"_s));
    QVERIFY(rejects(u"preset:Film:preset=Camera"_s));
    QVERIFY(rejects(u"preset:Film:cq=99"_s));
    QVERIFY(rejects(u"preset:Film:cq=high"_s));
    QVERIFY(rejects(u"preset:Film:tenbit=yes"_s));
}

void PresetSpecTest::matchKeepsThePickedChoiceWhileItHolds()
{
    QCOMPARE(match(u"Camera"_s, u"20"_s, u"off"_s), u"Camera"_s);
}

void PresetSpecTest::matchMovesToThePresetTheFieldsNowHold()
{
    // Film picked, then the quality turned down to 20: that is Camera now.
    QCOMPARE(match(u"Film"_s, u"20"_s, u"off"_s), u"Camera"_s);
}

void PresetSpecTest::matchFallsBackToTheChoiceWithoutAPreset()
{
    QCOMPARE(match(u"Film"_s, u"24"_s, u"off"_s), u"Custom"_s);
}

void PresetSpecTest::matchKeepsAChoiceWithoutAPreset()
{
    // Custom picked while the values are still Film's: the user wants to tweak, not to be sent back.
    QCOMPARE(match(u"Custom"_s, u"28"_s, u"off"_s), u"Custom"_s);
}

QTEST_GUILESS_MAIN(PresetSpecTest)

#include "presetspectest.moc"
