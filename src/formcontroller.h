// SPDX-FileCopyrightText: 2026 Pascal Klose
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fieldspec.h"
#include "presetspec.h"

#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQmlIntegration>

/// What the form window shows and edits, without any window: one value per field, kept in the
/// spelling the form prints (`on`/`off`, ISO dates, plain numbers). A preset ties one entry of a combo to
/// values of other fields: picking it sets them, and changing them re-selects the entry they match.
class FormController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main()")
    Q_PROPERTY(QString text READ text CONSTANT)
    Q_PROPERTY(QVariantList fields READ fields CONSTANT)
    Q_PROPERTY(QStringList values READ values NOTIFY valuesChanged)

public:
    FormController(const QString& text, const QList<FieldSpec>& fields, const QList<PresetSpec>& presets = {},
                   QObject* parent = nullptr);

    [[nodiscard]] QString text() const;

    /// One map per field for the window: name, type, label, choices, minimum, maximum.
    [[nodiscard]] QVariantList fields() const;

    /// The entered values, in the order the fields were given.
    [[nodiscard]] QStringList values() const;

    /// The entered values as `NAME=value` lines.
    [[nodiscard]] QStringList lines() const;

    [[nodiscard]] bool accepted() const;

    /// A value entered by the user: stored, then the presets are applied and matched.
    Q_INVOKABLE void setValue(int index, const QString& value);

    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();

Q_SIGNALS:
    void valuesChanged();

private:
    void write(int index, const QString& value);
    [[nodiscard]] int indexOf(const QString& name) const;
    void applyPreset(int comboIndex);
    void matchPresets();

    QString m_text;
    QList<FieldSpec> m_fields;
    QList<PresetSpec> m_presets;
    QStringList m_values;
    bool m_accepted = false;
};
