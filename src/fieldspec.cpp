// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fieldspec.h"

#include <KLocalizedString>

using namespace Qt::StringLiterals;

FieldType fieldTypeFromName(const QString& name)
{
    if (name == "text"_L1)
    {
        return FieldType::Text;
    }
    if (name == "pass"_L1 || name == "password"_L1)
    {
        return FieldType::Password;
    }
    if (name == "int"_L1)
    {
        return FieldType::Int;
    }
    if (name == "check"_L1)
    {
        return FieldType::Check;
    }
    if (name == "combo"_L1)
    {
        return FieldType::Combo;
    }
    if (name == "file"_L1)
    {
        return FieldType::File;
    }
    if (name == "dir"_L1)
    {
        return FieldType::Dir;
    }
    if (name == "date"_L1)
    {
        return FieldType::Date;
    }
    return FieldType::Invalid;
}

namespace
{
    bool parseRange(const QString& extra, int* minimum, int* maximum)
    {
        // Searching from index 1 so a negative lower bound keeps its sign: "-10-51" splits after "-10".
        // qsizetype rather than int because that is what QString indexes with - the narrowing clang-tidy
        // reports on `int` here is the 64-bit index being cut down, harmless for a command line
        // argument but pointless to write.
        const qsizetype separator = extra.indexOf(u'-', 1);

        if (separator <= 0)
        {
            return false;
        }

        bool lowOk = false;
        bool highOk = false;
        const int low = extra.left(separator).toInt(&lowOk);
        const int high = extra.mid(separator + 1).toInt(&highOk);

        if (!lowOk || !highOk || low > high)
        {
            return false;
        }

        *minimum = low;
        *maximum = high;
        return true;
    }
} // namespace

FieldSpec FieldSpec::parse(const QString& spec, QString* error)
{
    FieldSpec field;

    // The label may not contain a colon; the default and the extra may, so everything past the
    // fifth field is handed back to the extra unchanged.
    const QStringList parts = spec.split(u':');

    if (parts.size() < 3)
    {
        *error = i18n("Field \"%1\": expected NAME:TYPE:Label[:Default[:Extra]]", spec);
        return field;
    }

    const QString& typeName = parts.at(1);
    const FieldType type = fieldTypeFromName(typeName);

    if (type == FieldType::Invalid)
    {
        *error = i18n("Field \"%1\": unknown type \"%2\". Known: text, pass, int, check, combo, file, dir, date", spec,
                      typeName);
        return field;
    }

    if (parts.at(0).isEmpty())
    {
        *error = i18n("Field \"%1\": the name may not be empty, it is what the value is printed under", spec);
        return field;
    }

    field.name = parts.at(0);
    field.label = parts.at(2);
    field.defaultValue = parts.value(3);

    const QString extra = parts.size() > 5 ? QStringList(parts.mid(4)).join(u':') : parts.value(4);

    switch (type)
    {
        case FieldType::Combo:
        {
            const QStringList entries = field.defaultValue.split(u'|', Qt::SkipEmptyParts);

            if (entries.isEmpty())
            {
                *error = i18n("Field \"%1\": a combo needs its choices as Default, separated by \"|\"", spec);
                return field;
            }

            field.defaultValue.clear();

            for (const QString& entry : entries)
            {
                // A leading "*" marks the preselected choice; without one the first entry wins.
                if (entry.startsWith(u'*'))
                {
                    field.choices.append(entry.mid(1));
                    field.defaultValue = entry.mid(1);
                }
                else
                {
                    field.choices.append(entry);
                }
            }

            if (field.defaultValue.isEmpty())
            {
                field.defaultValue = field.choices.first();
            }
            break;
        }
        case FieldType::Int:
        {
            if (!extra.isEmpty() && !parseRange(extra, &field.minimum, &field.maximum))
            {
                *error = i18n("Field \"%1\": the range must read MIN-MAX, for example 10-51", spec);
                return field;
            }

            bool ok = false;
            const int value = field.defaultValue.toInt(&ok);

            field.number = ok ? qBound(field.minimum, value, field.maximum) : field.minimum;
            break;
        }
        case FieldType::Check:
            field.checked =
                field.defaultValue == "on"_L1 || field.defaultValue == "true"_L1 || field.defaultValue == "1"_L1;
            break;
        default:
            break;
    }

    field.type = type;
    return field;
}
