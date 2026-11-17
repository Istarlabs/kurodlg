// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#include "presetspec.h"

#include <KLocalizedString>

#include <QDate>

#include <algorithm>

using namespace Qt::StringLiterals;

PresetSpec PresetSpec::parse(const QString& spec, QString* error)
{
    PresetSpec preset;
    const QStringList parts = spec.split(u':');

    if (parts.size() < 3 || parts.at(0).isEmpty() || parts.at(1).isEmpty())
    {
        *error = i18n("Preset \"%1\": expected FIELD:CHOICE:NAME=VALUE[:NAME=VALUE...]", spec);
        return preset;
    }

    QList<std::pair<QString, QString>> values;

    for (const QString& assignment : parts.mid(2))
    {
        const qsizetype separator = assignment.indexOf(u'=');

        if (separator <= 0)
        {
            *error = i18n("Preset \"%1\": \"%2\" is not NAME=VALUE", spec, assignment);
            return preset;
        }

        values.append({assignment.left(separator), assignment.mid(separator + 1)});
    }

    preset.field = parts.at(0);
    preset.choice = parts.at(1);
    preset.values = values;
    return preset;
}

namespace
{
    const FieldSpec* findField(const QList<FieldSpec>& fields, const QString& name)
    {
        const auto found = std::ranges::find_if(fields, [&name](const FieldSpec& field) { return field.name == name; });
        return found == fields.end() ? nullptr : &*found;
    }

    /// `value` as the form would print it for `field`, or a null QString when the field cannot take it.
    QString normalized(const FieldSpec& field, const QString& value)
    {
        switch (field.type)
        {
            case FieldType::Int:
            {
                bool ok = false;
                const int number = value.toInt(&ok);
                return ok && number >= field.minimum && number <= field.maximum ? QString::number(number) : QString();
            }
            case FieldType::Check:
                if (value == "on"_L1 || value == "true"_L1 || value == "1"_L1)
                {
                    return u"on"_s;
                }
                if (value == "off"_L1 || value == "false"_L1 || value == "0"_L1)
                {
                    return u"off"_s;
                }
                return {};
            case FieldType::Combo:
                return field.choices.contains(value) ? value : QString();
            case FieldType::Date:
            {
                const QDate date = QDate::fromString(value, Qt::ISODate);
                return date.isValid() ? date.toString(Qt::ISODate) : QString();
            }
            default:
                // Not null even when empty: an empty text is a value a preset may set.
                return value.isNull() ? u""_s : value;
        }
    }
} // namespace

bool resolvePresets(QList<PresetSpec>* presets, const QList<FieldSpec>& fields, QString* error)
{
    for (PresetSpec& preset : *presets)
    {
        const FieldSpec* combo = findField(fields, preset.field);

        if (combo == nullptr || combo->type != FieldType::Combo)
        {
            *error = i18n("Preset for \"%1\": there is no combo field of that name", preset.field);
            return false;
        }

        if (!combo->choices.contains(preset.choice))
        {
            *error = i18n("Preset for \"%1\": \"%2\" is not one of its choices", preset.field, preset.choice);
            return false;
        }

        for (auto& [name, value] : preset.values)
        {
            const FieldSpec* target = findField(fields, name);

            if (target == nullptr || target == combo)
            {
                *error = i18n("Preset \"%1\": there is no other field named \"%2\"", preset.choice, name);
                return false;
            }

            const QString spelled = normalized(*target, value);

            if (spelled.isNull())
            {
                *error = i18n("Preset \"%1\": \"%2\" cannot take \"%3\"", preset.choice, name, value);
                return false;
            }

            value = spelled;
        }
    }

    return true;
}

QString matchingChoice(const FieldSpec& combo, const QString& current, const QList<PresetSpec>& presets,
                       const std::function<QString(const QString&)>& valueOf)
{
    const auto presetOf = [&](const QString& choice) -> const PresetSpec*
    {
        const auto found = std::ranges::find_if(presets, [&](const PresetSpec& preset)
                                                { return preset.field == combo.name && preset.choice == choice; });
        return found == presets.end() ? nullptr : &*found;
    };

    const auto holds = [&](const PresetSpec* preset)
    {
        return preset != nullptr &&
            std::ranges::all_of(preset->values,
                                [&](const auto& assignment) { return valueOf(assignment.first) == assignment.second; });
    };

    // A choice without a preset is the user saying "my own values", and two presets may hold the same
    // values - either way, what was picked is what keeps showing.
    if (const PresetSpec* picked = presetOf(current); picked == nullptr || holds(picked))
    {
        return current;
    }

    for (const QString& choice : combo.choices)
    {
        if (holds(presetOf(choice)))
        {
            return choice;
        }
    }

    for (const QString& choice : combo.choices)
    {
        if (presetOf(choice) == nullptr)
        {
            return choice;
        }
    }

    return {};
}
