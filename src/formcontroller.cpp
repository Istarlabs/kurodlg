// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#include "formcontroller.h"

#include <QCoreApplication>
#include <QDate>
#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace
{
    /// The name the window switches on; the order of FieldType does not leak into QML.
    QString typeName(const FieldType type)
    {
        switch (type)
        {
            case FieldType::Password:
                return u"password"_s;
            case FieldType::Int:
                return u"int"_s;
            case FieldType::Check:
                return u"check"_s;
            case FieldType::Combo:
                return u"combo"_s;
            case FieldType::File:
                return u"file"_s;
            case FieldType::Dir:
                return u"dir"_s;
            case FieldType::Date:
                return u"date"_s;
            case FieldType::Text:
            case FieldType::Invalid:
                break;
        }

        return u"text"_s;
    }

    /// What the field starts with.
    QString initialValue(const FieldSpec& field)
    {
        switch (field.type)
        {
            case FieldType::Int:
                return QString::number(qBound(field.minimum, field.number, field.maximum));
            case FieldType::Check:
                return field.checked ? u"on"_s : u"off"_s;
            case FieldType::Combo:
                // The default only counts when it is one of the choices; otherwise the combo opens on its
                // first entry, as a read-only combo does.
                return field.choices.contains(field.defaultValue) || field.choices.isEmpty() ? field.defaultValue
                                                                                             : field.choices.first();
            case FieldType::Date:
            {
                const QDate parsed = QDate::fromString(field.defaultValue, Qt::ISODate);
                return (parsed.isValid() ? parsed : QDate::currentDate()).toString(Qt::ISODate);
            }
            default:
                return field.defaultValue;
        }
    }
} // namespace

FormController::FormController(const QString& text, const QList<FieldSpec>& fields, const QList<PresetSpec>& presets,
                               QObject* parent)
    : QObject(parent), m_text(text), m_fields(fields), m_presets(presets)
{
    for (const FieldSpec& field : std::as_const(m_fields))
    {
        m_values.append(initialValue(field));
    }

    // The defaults win over the preselected choice: the combo opens on whatever they amount to.
    matchPresets();
}

QString FormController::text() const
{
    return m_text;
}

QVariantList FormController::fields() const
{
    QVariantList list;

    for (const FieldSpec& field : m_fields)
    {
        list.append(QVariantMap{
            {u"name"_s, field.name},
            {u"type"_s, typeName(field.type)},
            {u"label"_s, field.label},
            {u"choices"_s, field.choices},
            {u"minimum"_s, field.minimum},
            {u"maximum"_s, field.maximum},
        });
    }

    return list;
}

QStringList FormController::values() const
{
    return m_values;
}

QStringList FormController::lines() const
{
    QStringList result;

    for (int index = 0; index < m_fields.size(); ++index)
    {
        result.append(m_fields.at(index).name + u'=' + m_values.at(index));
    }

    return result;
}

bool FormController::accepted() const
{
    return m_accepted;
}

int FormController::indexOf(const QString& name) const
{
    for (int index = 0; index < m_fields.size(); ++index)
    {
        if (m_fields.at(index).name == name)
        {
            return index;
        }
    }

    return -1;
}

/// Stores `value` the way the field would hold it: a number inside its range, `on` or `off`, a choice that
/// exists, a date that parses. Anything else leaves the field as it is.
void FormController::write(const int index, const QString& value)
{
    if (index < 0 || index >= m_fields.size())
    {
        return;
    }

    const FieldSpec& field = m_fields.at(index);
    QString stored = value;

    switch (field.type)
    {
        case FieldType::Int:
            stored = QString::number(qBound(field.minimum, value.toInt(), field.maximum));
            break;
        case FieldType::Check:
            stored = value == "on"_L1 ? u"on"_s : u"off"_s;
            break;
        case FieldType::Combo:
            if (!field.choices.contains(value))
            {
                return;
            }
            break;
        case FieldType::Date:
            if (!QDate::fromString(value, Qt::ISODate).isValid())
            {
                return;
            }
            break;
        default:
            break;
    }

    m_values[index] = stored;
}

void FormController::setValue(const int index, const QString& value)
{
    write(index, value);
    applyPreset(index);
    matchPresets();

    Q_EMIT valuesChanged();
}

void FormController::applyPreset(const int comboIndex)
{
    const FieldSpec& field = m_fields.at(comboIndex);

    if (field.type != FieldType::Combo)
    {
        return;
    }

    const QString choice = m_values.at(comboIndex);

    for (const PresetSpec& preset : std::as_const(m_presets))
    {
        if (preset.field != field.name || preset.choice != choice)
        {
            continue;
        }

        for (const auto& [name, value] : preset.values)
        {
            write(indexOf(name), value);
        }
    }
}

void FormController::matchPresets()
{
    for (int index = 0; index < m_fields.size(); ++index)
    {
        const FieldSpec& field = m_fields.at(index);

        if (field.type != FieldType::Combo)
        {
            continue;
        }

        const QString choice = matchingChoice(field, m_values.at(index), m_presets,
                                              [this](const QString& name)
                                              {
                                                  const int at = indexOf(name);
                                                  return at < 0 ? QString() : m_values.at(at);
                                              });

        if (!choice.isEmpty())
        {
            write(index, choice);
        }
    }
}

void FormController::accept()
{
    m_accepted = true;
    QCoreApplication::quit();
}

void FormController::reject()
{
    QCoreApplication::quit();
}

#include "moc_formcontroller.cpp"
