// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

#pragma once

#include <KQuickConfigModule>

class SwapKcm : public KQuickConfigModule
{
    Q_OBJECT

    Q_PROPERTY(QVariantList swapEntries READ swapEntries NOTIFY swapEntriesChanged)
    Q_PROPERTY(QString zramSize READ zramSize WRITE setZramSize NOTIFY zramSizeChanged)
    Q_PROPERTY(QString zramAlgorithm READ zramAlgorithm WRITE setZramAlgorithm NOTIFY zramAlgorithmChanged)
    Q_PROPERTY(QStringList availableAlgorithms READ availableAlgorithms NOTIFY availableAlgorithmsChanged)
    Q_PROPERTY(QString zramCurrentSize READ zramCurrentSize NOTIFY zramCurrentSizeChanged)
    Q_PROPERTY(QString zramOrigDataSize READ zramOrigDataSize NOTIFY zramOrigDataSizeChanged)
    Q_PROPERTY(QString zramComprDataSize READ zramComprDataSize NOTIFY zramComprDataSizeChanged)
    Q_PROPERTY(QString totalRam READ totalRam NOTIFY totalRamChanged)
    Q_PROPERTY(QString swappiness READ swappiness NOTIFY swappinessChanged)

public:
    explicit SwapKcm(QObject *parent, const KPluginMetaData &metaData);

    QVariantList swapEntries() const { return m_swapEntries; }

    QString zramSize() const { return m_zramSize; }
    void setZramSize(const QString &value);

    QString zramAlgorithm() const { return m_zramAlgorithm; }
    void setZramAlgorithm(const QString &value);

    QStringList availableAlgorithms() const { return m_availableAlgorithms; }

    QString zramCurrentSize() const { return m_zramCurrentSize; }
    QString zramOrigDataSize() const { return m_zramOrigDataSize; }
    QString zramComprDataSize() const { return m_zramComprDataSize; }
    QString totalRam() const { return m_totalRam; }
    QString swappiness() const { return m_swappiness; }

public Q_SLOTS:
    void save() override;
    void load() override;

Q_SIGNALS:
    void swapEntriesChanged();
    void zramSizeChanged();
    void zramAlgorithmChanged();
    void availableAlgorithmsChanged();
    void zramCurrentSizeChanged();
    void zramOrigDataSizeChanged();
    void zramComprDataSizeChanged();
    void totalRamChanged();
    void swappinessChanged();

private:
    void loadSwapEntries();
    void loadZramConfig();
    void loadAvailableAlgorithms();
    void loadZramStats();
    void loadMemInfo();
    void loadSwappiness();

    static QString formatBytes(qint64 bytes);

    QVariantList m_swapEntries;

    QString m_zramSize;
    QString m_zramAlgorithm;
    QStringList m_availableAlgorithms;

    QString m_zramCurrentSize;
    QString m_zramOrigDataSize;
    QString m_zramComprDataSize;
    QString m_totalRam;
    QString m_swappiness;

    // Originals for change detection
    QString m_origZramSize;
    QString m_origZramAlgorithm;
};
