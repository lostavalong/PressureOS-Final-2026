#include "PinyinInputEngine.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace {
constexpr int kMaximumCompositionLength = 24;
constexpr int kMaximumCandidates = 10;

QString normalizedPinyin(QString value)
{
    value = value.trimmed().toLower();
    value.remove(QRegularExpression(QStringLiteral("[^a-z]")));
    return value;
}
}

PinyinInputEngine::PinyinInputEngine(QObject *parent)
    : QObject(parent)
{
    addCoreEntries();
    loadLexicon();
    m_ready = !m_entries.isEmpty();
}

QString PinyinInputEngine::statusText() const
{
    return m_ready ? QStringLiteral("离线拼音词库已就绪")
                   : QStringLiteral("基础拼音词库不可用");
}

void PinyinInputEngine::setChineseMode(bool enabled)
{
    if (m_chineseMode == enabled)
        return;
    m_chineseMode = enabled;
    m_composition.clear();
    m_candidates.clear();
    emit stateChanged();
}

void PinyinInputEngine::appendLetter(const QString &letter)
{
    if (!m_chineseMode || m_composition.size() >= kMaximumCompositionLength)
        return;
    const QString normalized = normalizedPinyin(letter);
    if (normalized.size() != 1)
        return;
    m_composition += normalized;
    updateCandidates();
    emit stateChanged();
}

void PinyinInputEngine::backspace()
{
    if (m_composition.isEmpty())
        return;
    m_composition.chop(1);
    updateCandidates();
    emit stateChanged();
}

void PinyinInputEngine::clear()
{
    if (m_composition.isEmpty() && m_candidates.isEmpty())
        return;
    m_composition.clear();
    m_candidates.clear();
    emit stateChanged();
}

QString PinyinInputEngine::takeCandidate(int index)
{
    if (index < 0 || index >= m_candidates.size())
        return {};
    const QString result = m_candidates.at(index);
    clear();
    return result;
}

QString PinyinInputEngine::takeFirstCandidate()
{
    if (m_composition.isEmpty())
        return {};
    const QString result = m_candidates.isEmpty() ? m_composition : m_candidates.first();
    clear();
    return result;
}

void PinyinInputEngine::loadLexicon()
{
    QFile file(QStringLiteral(":/qt/qml/PressureOS/data/input/pinyin_lexicon.tsv"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList fields = line.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
        if (fields.size() < 2)
            continue;
        const QString key = normalizedPinyin(fields.first());
        if (key.isEmpty())
            continue;
        QStringList &target = m_entries[key];
        for (int i = 1; i < fields.size() && target.size() < kMaximumCandidates; ++i) {
            const QString candidate = fields.at(i).trimmed();
            if (!candidate.isEmpty() && !target.contains(candidate))
                target.push_back(candidate);
        }
    }
}

void PinyinInputEngine::addCoreEntries()
{
    const QList<QPair<QString, QStringList>> core = {
        {QStringLiteral("renwu"), {QStringLiteral("任务"), QStringLiteral("人物")}},
        {QStringLiteral("yali"), {QStringLiteral("压力"), QStringLiteral("雅丽")}},
        {QStringLiteral("biaoding"), {QStringLiteral("标定"), QStringLiteral("表定")}},
        {QStringLiteral("xiaozhun"), {QStringLiteral("校准")}},
        {QStringLiteral("wendu"), {QStringLiteral("温度")}},
        {QStringLiteral("changwen"), {QStringLiteral("常温")}},
        {QStringLiteral("wending"), {QStringLiteral("稳定")}},
        {QStringLiteral("shuju"), {QStringLiteral("数据")}},
        {QStringLiteral("celiang"), {QStringLiteral("测量")}},
        {QStringLiteral("shiyan"), {QStringLiteral("实验"), QStringLiteral("试验")}},
        {QStringLiteral("muban"), {QStringLiteral("模板")}},
        {QStringLiteral("caiji"), {QStringLiteral("采集")}},
        {QStringLiteral("fenxi"), {QStringLiteral("分析")}},
        {QStringLiteral("jieguo"), {QStringLiteral("结果")}},
        {QStringLiteral("daochu"), {QStringLiteral("导出")}},
        {QStringLiteral("lingdian"), {QStringLiteral("零点")}},
        {QStringLiteral("liangcheng"), {QStringLiteral("量程")}},
        {QStringLiteral("bupinheng"), {QStringLiteral("不平衡")}},
        {QStringLiteral("buchang"), {QStringLiteral("补偿")}},
        {QStringLiteral("wendubuchang"), {QStringLiteral("温度补偿")}},
        {QStringLiteral("buxingdu"), {QStringLiteral("不确定度")}},
        {QStringLiteral("buquedingdu"), {QStringLiteral("不确定度")}},
        {QStringLiteral("xianxingdu"), {QStringLiteral("线性度")}},
        {QStringLiteral("chongfuxing"), {QStringLiteral("重复性")}},
        {QStringLiteral("huichengwucha"), {QStringLiteral("回程误差")}},
        {QStringLiteral("shizhuwucha"), {QStringLiteral("示值误差")}},
        {QStringLiteral("xieya"), {QStringLiteral("泄压")}},
        {QStringLiteral("chaoya"), {QStringLiteral("超压")}},
        {QStringLiteral("chuankou"), {QStringLiteral("串口")}},
        {QStringLiteral("xieyi"), {QStringLiteral("协议")}},
        {QStringLiteral("wanzhengxing"), {QStringLiteral("完整性")}},
        {QStringLiteral("kexin"), {QStringLiteral("可信")}},
        {QStringLiteral("kexinlian"), {QStringLiteral("可信链")}},
        {QStringLiteral("shangweiji"), {QStringLiteral("上位机")}},
        {QStringLiteral("xiaweiji"), {QStringLiteral("下位机")}},
        {QStringLiteral("shumeipai"), {QStringLiteral("树莓派")}}
    };
    for (const auto &entry : core)
        m_entries.insert(entry.first, entry.second);
}

void PinyinInputEngine::updateCandidates()
{
    m_candidates.clear();
    if (m_composition.isEmpty())
        return;

    const auto exact = m_entries.constFind(m_composition);
    if (exact != m_entries.cend())
        m_candidates = exact.value().mid(0, kMaximumCandidates);

    if (m_candidates.size() >= kMaximumCandidates || m_composition.size() < 2)
        return;

    auto it = m_entries.lowerBound(m_composition);
    int inspected = 0;
    while (it != m_entries.cend() && it.key().startsWith(m_composition)
           && m_candidates.size() < kMaximumCandidates && inspected < 120) {
        for (const QString &candidate : it.value()) {
            if (!m_candidates.contains(candidate))
                m_candidates.push_back(candidate);
            if (m_candidates.size() >= kMaximumCandidates)
                break;
        }
        ++it;
        ++inspected;
    }
}
