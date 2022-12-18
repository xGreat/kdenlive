/*
   SPDX-FileCopyrightText: 2017 Nicolas Carion
   SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
   SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
   This file is part of Kdenlive. See www.kdenlive.org.
*/

#include "renderpresetmodel.hpp"
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "renderpresetrepository.hpp"

#include <KLocalizedString>
#include <KMessageWidget>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <memory>

QString RenderPresetParams::toString()
{
    QString string;
    RenderPresetParams::const_iterator i = this->constBegin();
    while (i != this->constEnd()) {
        string.append(QStringLiteral("%1=%2 ").arg(i.key(), i.value()));
        ++i;
    }
    return string.simplified();
}

void RenderPresetParams::replacePlaceholder(const QString &placeholder, const QString &newValue)
{
    RenderPresetParams newParams;
    RenderPresetParams::const_iterator i = this->constBegin();
    while (i != this->constEnd()) {
        QString value = i.value();
        newParams.insert(i.key(), value.replace(placeholder, newValue));
        ++i;
    }

    clear();
    QMap<QString, QString>::const_iterator j = newParams.constBegin();
    while (j != newParams.constEnd()) {
        insert(j.key(), j.value());
        ++j;
    }
}

RenderPresetModel::RenderPresetModel(QDomElement preset, const QString &presetFile, bool editable, const QString &groupName, const QString &renderer)
    : m_presetFile(presetFile)
    , m_editable(editable)
    , m_name(preset.attribute(QStringLiteral("name")))
    , m_note()
    , m_standard(preset.attribute(QStringLiteral("standard")))
    , m_params()
    , m_groupName(preset.attribute(QStringLiteral("category"), groupName))
    , m_renderer(renderer)
    , m_url(preset.attribute(QStringLiteral("url")))
    , m_speeds(preset.attribute(QStringLiteral("speeds")))
    , m_defaultSpeedIndex(preset.attribute(QStringLiteral("defaultspeedindex"), QStringLiteral("-1")).toInt())
    , m_topFieldFirst(preset.attribute(QStringLiteral("top_field_first")))
{
    if (m_groupName.isEmpty()) {
        m_groupName = i18nc("Category Name", "Custom");
    }

    m_extension = preset.attribute(QStringLiteral("extension"));
    m_manual = preset.attribute(QStringLiteral("manual")) == QLatin1String("1");

    QTextDocument docConvert;
    docConvert.setHtml(preset.attribute(QStringLiteral("args")));
    // setParams after we know the extension to make setting the format automatically work
    setParams(docConvert.toPlainText().simplified());

    if (m_defaultSpeedIndex < 0) {
        m_defaultSpeedIndex = (speeds().count() - 1) * 0.75;
    }

    m_vQualities = preset.attribute(QStringLiteral("qualities"));
    m_defaultVQuality = preset.attribute(QStringLiteral("defaultquality"));
    m_vBitrates = preset.attribute(QStringLiteral("bitrates"));
    m_defaultVBitrate = preset.attribute(QStringLiteral("defaultbitrate"));

    m_aQualities = preset.attribute(QStringLiteral("audioqualities"));
    m_defaultAQuality = preset.attribute(QStringLiteral("defaultaudioquality"));
    m_aBitrates = preset.attribute(QStringLiteral("audiobitrates"));
    m_defaultABitrate = preset.attribute(QStringLiteral("defaultaudiobitrate"));

    checkPreset();
}

RenderPresetModel::RenderPresetModel(const QString &groupName, const QString &path, QString presetName, const QString &params, bool codecInName)
    : m_editable(false)
    , m_manual(false)
    , m_params()
    , m_groupName(groupName)
    , m_renderer(QStringLiteral("avformat"))
    , m_defaultSpeedIndex(-1)
{
    KConfig config(path, KConfig::SimpleConfig);
    KConfigGroup group = config.group(QByteArray());
    QString vcodec = group.readEntry("vcodec");
    QString acodec = group.readEntry("acodec");
    m_extension = group.readEntry("meta.preset.extension");
    // setParams after we know the extension to make setting the format automatically work
    setParams(params);
    m_note = group.readEntry("meta.preset.note");

    if (codecInName && (!vcodec.isEmpty() || !acodec.isEmpty())) {
        presetName.append(" (");
        if (!vcodec.isEmpty()) {
            presetName.append(vcodec);
            if (!acodec.isEmpty()) {
                presetName.append("+" + acodec);
            }
        } else if (!acodec.isEmpty()) {
            presetName.append(acodec);
        }
        presetName.append(QLatin1Char(')'));
    }
    m_name = presetName;
    checkPreset();
}

RenderPresetModel::RenderPresetModel(const QString &name, const QString &groupName, const QString &params, const QString &extension,
                                     const QString &defaultVBitrate, const QString &defaultVQuality, const QString &defaultABitrate,
                                     const QString &defaultAQuality, const QString &speedsString, bool manualPreset)
    : m_presetFile()
    , m_editable()
    , m_name(name)
    , m_note()
    , m_standard()
    , m_extension(extension)
    , m_groupName(groupName)
    , m_renderer(QStringLiteral("avformat"))
    , m_url()
    , m_speeds(speedsString)
    , m_topFieldFirst()
    , m_vBitrates()
    , m_defaultVBitrate(defaultVBitrate)
    , m_vQualities()
    , m_defaultVQuality(defaultVQuality)
    , m_aBitrates()
    , m_defaultABitrate(defaultABitrate)
    , m_aQualities()
    , m_defaultAQuality(defaultAQuality)
{
    setParams(params);
    if (m_groupName.isEmpty()) {
        m_groupName = i18nc("Category Name", "Custom");
    }

    m_defaultSpeedIndex = (speeds().count() - 1) * 0.75;
    m_manual = manualPreset;

    checkPreset();
}

void RenderPresetModel::checkPreset()
{
    QStringList acodecs = RenderPresetRepository::acodecs();
    QStringList vcodecs = RenderPresetRepository::vcodecs();
    QStringList supportedFormats = RenderPresetRepository::supportedFormats();

    bool replaceVorbisCodec = acodecs.contains(QStringLiteral("libvorbis"));
    bool replaceLibfaacCodec = acodecs.contains(QStringLiteral("libfaac"));

    if (replaceVorbisCodec && (m_params.value(QStringLiteral("acodec")) == QStringLiteral("vorbis"))) {
        // replace vorbis with libvorbis
        m_params[QStringLiteral("acodec")] = QLatin1String("libvorbis");
    }
    if (replaceLibfaacCodec && (m_params.value(QStringLiteral("acodec")) == QStringLiteral("aac"))) {
        // replace aac with libfaac
        m_params[QStringLiteral("acodec")] = QLatin1String("libfaac");
    }

    // We borrow a reference to the profile's pointer to query it more easily
    std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
    double project_framerate = double(profile->frame_rate_num()) / profile->frame_rate_den();

    if (m_standard.isEmpty() ||
        (m_standard.contains(QStringLiteral("PAL"), Qt::CaseInsensitive) && profile->frame_rate_num() == 25 && profile->frame_rate_den() == 1) ||
        (m_standard.contains(QStringLiteral("NTSC"), Qt::CaseInsensitive) && profile->frame_rate_num() == 30000 && profile->frame_rate_den() == 1001)) {
        // Standard OK
    } else {
        m_errors = i18n("Standard (%1) not compatible with project profile (%2)", m_standard, project_framerate);
        return;
    }

    if (hasParam(QStringLiteral("mlt_profile"))) {
        QString profile_str = getParam(QStringLiteral("mlt_profile"));
        std::unique_ptr<ProfileModel> &target_profile = ProfileRepository::get()->getProfile(profile_str);
        if (target_profile->frame_rate_den() > 0) {
            double profile_rate = double(target_profile->frame_rate_num()) / target_profile->frame_rate_den();
            if (int(1000.0 * profile_rate) != int(1000.0 * project_framerate)) {
                m_errors = i18n("Frame rate (%1) not compatible with project profile (%2)", profile_rate, project_framerate);
                return;
            }
        }
    }

    // Make sure the selected preset uses an installed avformat codec / format
    QString format;
    format = getParam(QStringLiteral("f")).toLower();
    if (!format.isEmpty() && !supportedFormats.contains(format)) {
        m_errors = i18n("Unsupported video format: %1", format);
        return;
    }

    // check for missing audio codecs
    format = getParam(QStringLiteral("acodec")).toLower();
    if (!format.isEmpty() && !acodecs.contains(format)) {
        m_errors = i18n("Unsupported audio codec: %1", format);
        return;
    }
    // check for missing video codecs
    format = getParam(QStringLiteral("vcodec")).toLower();
    if (!format.isEmpty() && !vcodecs.contains(format)) {
        m_errors = i18n("Unsupported video codec: %1", format);
        return;
    }

    if (hasParam(QStringLiteral("profile"))) {
        m_warnings = i18n("This render preset uses a 'profile' parameter.<br />Unless you know what you are doing you will probably "
                          "have to change it to 'mlt_profile'.");
        return;
    }
}

QDomElement RenderPresetModel::toXml()
{
    QDomDocument doc;
    QDomElement profileElement = doc.createElement(QStringLiteral("profile"));
    doc.appendChild(profileElement);

    profileElement.setAttribute(QStringLiteral("name"), m_name);
    profileElement.setAttribute(QStringLiteral("category"), m_groupName);
    if (!m_extension.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("extension"), m_extension);
    }
    if (m_manual) {
        profileElement.setAttribute(QStringLiteral("manual"), QStringLiteral("1"));
    }
    profileElement.setAttribute(QStringLiteral("args"), m_params.toString());
    if (!m_defaultVBitrate.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("defaultbitrate"), m_defaultVBitrate);
    }
    if (!m_vBitrates.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("bitrates"), m_vBitrates);
    }
    if (!m_defaultVQuality.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("defaultquality"), m_defaultVQuality);
    }
    if (!m_vQualities.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("qualities"), m_vQualities);
    }
    if (!m_defaultABitrate.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("defaultaudiobitrate"), m_defaultABitrate);
    }
    if (!m_aBitrates.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("audiobitrates"), m_aBitrates);
    }
    if (!m_defaultAQuality.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("defaultaudioquality"), m_defaultAQuality);
    }
    if (!m_aQualities.isEmpty()) {
        profileElement.setAttribute(QStringLiteral("audioqualities"), m_aQualities);
    }
    if (!m_speeds.isEmpty()) {
        // profile has a variable speed
        profileElement.setAttribute(QStringLiteral("speeds"), m_speeds);
        if (m_defaultSpeedIndex > 0) {
            profileElement.setAttribute(QStringLiteral("defaultspeedindex"), m_defaultSpeedIndex);
        }
    }
    return doc.documentElement();
}

void RenderPresetModel::setParams(const QString &params)
{
    m_params.clear();
    // split only at white spaces followed by a new parameter
    // to avoid spliting values that contain whitespaces
    static const QRegularExpression regexp(R"(\s+(?=\S*=))");

    QStringList paramList = params.split(regexp);
    for (QString param : paramList) {
        m_params.insert(param.section(QLatin1Char('='), 0, 0), param.section(QLatin1Char('='), 1));
    }
    if (!hasParam(QStringLiteral("f")) && !m_extension.isEmpty() && RenderPresetRepository::supportedFormats().contains(m_extension)) {
        m_params.insert(QStringLiteral("f"), m_extension);
    }
}

RenderPresetParams RenderPresetModel::params(QStringList removeParams) const
{
    RenderPresetParams newParams = m_params;
    for (QString toRemove : removeParams) {
        newParams.remove(toRemove);
    }
    return newParams;
}

QString RenderPresetModel::extension() const
{
    if (!m_extension.isEmpty()) {
        return m_extension;
    }
    return getParam(QStringLiteral("f"));
};

QStringList RenderPresetModel::audioBitrates() const
{
    return m_aBitrates.split(QLatin1Char(','), Qt::SkipEmptyParts);
}

QString RenderPresetModel::defaultABitrate() const
{
    return m_defaultABitrate;
}

QStringList RenderPresetModel::audioQualities() const
{
    if (!m_aQualities.isEmpty()) {
        return m_aQualities.split(QLatin1Char(','), Qt::SkipEmptyParts);
    } else {
        // ATTENTION: historically qualities are sorted from best to worse for some reason
        QString acodec = getParam(QStringLiteral("acodec")).toLower();
        if (acodec == "libmp3lame") {
            return {"0", "9"};
        } else if (acodec == "libvorbis" || acodec == "vorbis" || acodec == "libopus") {
            return {"10", "0"};
        } else {
            return {"500", "0"};
        }
    }
}

QString RenderPresetModel::defaultAQuality() const
{
    return m_defaultAQuality;
}

QStringList RenderPresetModel::videoBitrates() const
{
    return m_vBitrates.split(QLatin1Char(','), Qt::SkipEmptyParts);
}

QString RenderPresetModel::defaultVBitrate() const
{
    return m_defaultVBitrate;
}

QStringList RenderPresetModel::videoQualities() const
{
    if (!m_vQualities.isEmpty()) {
        return m_vQualities.split(QLatin1Char(','), Qt::SkipEmptyParts);
    } else {
        // ATTENTION: historically qualities are sorted from best to worse for some reason
        QString vcodec = getParam(QStringLiteral("vcodec")).toLower();
        if (vcodec == "libx265" || vcodec.contains("nvenc") || vcodec.endsWith("_amf") || vcodec.startsWith("libx264") || vcodec.endsWith("_vaapi") ||
            vcodec.endsWith("_qsv")) {
            return {"0", "51"};
        } else if (vcodec.startsWith("libvpx") || vcodec.startsWith("libaom-")) {
            return {"0", "63"};
        } else if (vcodec.startsWith("libwebp")) {
            return {"100", "0"};
        } else {
            return {"1", "31"};
        }
    }
}

QString RenderPresetModel::defaultVQuality() const
{
    return m_defaultVQuality;
}

bool RenderPresetModel::editable() const
{
    return m_editable;
}

QStringList RenderPresetModel::speeds() const
{
    return m_speeds.split(QLatin1Char(';'), Qt::SkipEmptyParts);
}

QString RenderPresetModel::getParam(const QString &name) const
{
    return params().value(name);
}

bool RenderPresetModel::isManual() const
{
    return m_manual;
}

bool RenderPresetModel::hasParam(const QString &name) const
{
    // return params().contains(name);
    return !getParam(name).isEmpty();
}

RenderPresetModel::RateControl RenderPresetModel::videoRateControl() const
{
    QString vbufsize = getParam(QStringLiteral("vbufsize"));
    QString vcodec = getParam(QStringLiteral("vcodec"));
    if (hasParam(QStringLiteral("crf"))) {
        return !vbufsize.isEmpty() ? (vcodec.endsWith("_videotoolbox") ? RateControl::Average : RateControl::Quality) : RateControl::Constrained;
    }
    if (hasParam(QStringLiteral("vq")) || hasParam(QStringLiteral("vglobal_quality")) || hasParam(QStringLiteral("qscale"))) {
        return vbufsize.isEmpty() ? RateControl::Quality : RateControl::Constrained;
    } else if (!vbufsize.isEmpty()) {
        return RateControl::Constant;
    }
    QString param = getParam(QStringLiteral("vb")).replace('+', "").replace('k', "").replace('k', "").replace('M', "000");
    qDebug() << "vb param" << param << param.toInt();
    if (!param.isEmpty()) {
        return (param.contains(QStringLiteral("%bitrate")) || param.toInt() > 0) ? RateControl::Average : RateControl::Quality;
    }
    return RateControl::Unknown;
}

RenderPresetModel::RateControl RenderPresetModel::audioRateControl() const
{
    QString value = getParam(QStringLiteral("vbr"));
    if (!value.isEmpty()) {
        // libopus rate mode
        if (value == QStringLiteral("off")) {
            return RateControl::Constant;
        }
        if (value == QStringLiteral("constrained")) {
            return RateControl::Average;
        }
        return RateControl::Quality;
    }
    if (hasParam(QStringLiteral("aq")) || hasParam(QStringLiteral("compression_level"))) {
        return RateControl::Quality;
    }
    if (hasParam(QStringLiteral("ab"))) {
        return RateControl::Constant;
    }
    return RateControl::Unknown;
}

QString RenderPresetModel::x265Params() const
{
    QString x265Params = getParam(QStringLiteral("x265-params"));
    QStringList newX265Params;
    if (hasParam(QStringLiteral("crf"))) {
        newX265Params.append(QStringLiteral("crf=%1").arg(getParam(QStringLiteral("crf"))));
    }
    if (videoRateControl() == RateControl::Constant && hasParam(QStringLiteral("vb"))) {
        QString vb = getParam(QStringLiteral("vb"));
        QString newVal;
        if (vb == QStringLiteral("%bitrate") || vb == QStringLiteral("%bitrate+'k")) {
            newVal = QStringLiteral("%bitrate");
        } else {
            newVal = vb.replace('k', QLatin1String("")).replace('M', QStringLiteral("000"));
        }
        newX265Params.append(QStringLiteral("bitrate=%1").arg(newVal));
        newX265Params.append(QStringLiteral("vbv-maxrate=%1").arg(newVal));
    } else if (videoRateControl() == RateControl::Constrained && hasParam(QStringLiteral("vmaxrate"))) {
        QString vmax = getParam(QStringLiteral("vmaxrate"));
        QString newVal;
        if (vmax == QStringLiteral("%bitrate") || vmax == QStringLiteral("%bitrate+'k")) {
            newVal = QStringLiteral("%bitrate");
        } else {
            newVal = vmax.replace('k', QLatin1String("")).replace('M', QStringLiteral("000"));
        }
        newX265Params.append(QStringLiteral("vbv-maxrate=%1").arg(newVal));
    }
    if (hasParam(QStringLiteral("vbufsize"))) {
        int val = getParam(QStringLiteral("vbufsize")).toInt();
        newX265Params.append(QStringLiteral("vbv-bufsize=%1").arg(val / 1024));
    }
    if (hasParam(QStringLiteral("bf"))) {
        newX265Params.append(QStringLiteral("bframes=%1").arg(getParam(QStringLiteral("bf"))));
    }
    if (hasParam(QStringLiteral("g"))) {
        newX265Params.append(QStringLiteral("keyint=%1").arg(getParam(QStringLiteral("g"))));
    }
    if (hasParam(QStringLiteral("top_field_first")) && getParam(QStringLiteral("progressive")).toInt() == 0) {
        int val = getParam(QStringLiteral("top_field_first")).toInt();
        newX265Params.append(QStringLiteral("interlace=%1").arg(val == 1 ? QStringLiteral("tff") : QStringLiteral("bff")));
    }
    if (hasParam(QStringLiteral("sc_threshold")) && getParam(QStringLiteral("sc_threshold")).toInt() == 0) {
        newX265Params.append(QStringLiteral("scenecut=0"));
    }
    if (!x265Params.isEmpty()) {
        newX265Params.append(x265Params);
    }
    return newX265Params.join(QStringLiteral(":"));
}

RenderPresetModel::InstallType RenderPresetModel::installType() const
{
    if (m_editable) {
        if (m_presetFile.endsWith(QLatin1String("customprofiles.xml"))) {
            return InstallType::Custom;
        } else {
            return InstallType::Download;
        }
    }
    return InstallType::BuildIn;
}

bool RenderPresetModel::hasFixedSize() const
{
    // TODO
    return hasParam(QStringLiteral("s")) || m_params.contains(QLatin1String("%dv_standard"));
}

QString RenderPresetModel::error() const
{
    return m_errors;
}

QString RenderPresetModel::warning() const
{
    return m_warnings;
}

int RenderPresetModel::estimateFileSize(int length)
{
    Q_UNUSED(length)
    // TODO
    /*RateControl vrc = videoRateControl();
    if (vrc == Average || vrc == Constant) {

        return m_defaultVBitrate.toInt() * length;
    }*/
    return -1;
}
