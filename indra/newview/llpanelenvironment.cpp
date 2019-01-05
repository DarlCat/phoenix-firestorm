/** 
 * @file llpanelenvironment.cpp
 * @brief LLPanelExperiences class implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2013, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"


#include "llpanelprofile.h"
#include "lluictrlfactory.h"
#include "llexperiencecache.h"
#include "llagent.h"
#include "llparcel.h"

#include "llviewerregion.h"
#include "llpanelenvironment.h"
#include "llslurl.h"
#include "lllayoutstack.h"

#include "llfloater.h"
#include "llfloaterreg.h"
#include "llfloatereditextdaycycle.h"
#include "llmultisliderctrl.h"
#include "llsettingsvo.h"

#include "llappviewer.h"
#include "llcallbacklist.h"
#include "llviewerparcelmgr.h"

#include "llinventorymodel.h"

//=========================================================================
namespace 
{
    const std::string FLOATER_DAY_CYCLE_EDIT("env_edit_extdaycycle");

    inline bool ends_with(std::string const & value, std::string const & ending)
    {
        if (ending.size() > value.size())
            return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    }

}

//=========================================================================
const std::string LLPanelEnvironmentInfo::BTN_SELECTINV("btn_select_inventory");
const std::string LLPanelEnvironmentInfo::BTN_EDIT("btn_edit");
const std::string LLPanelEnvironmentInfo::BTN_USEDEFAULT("btn_usedefault");
const std::string LLPanelEnvironmentInfo::SLD_DAYLENGTH("sld_day_length");
const std::string LLPanelEnvironmentInfo::SLD_DAYOFFSET("sld_day_offset");
const std::string LLPanelEnvironmentInfo::SLD_ALTITUDES("sld_altitudes");
const std::string LLPanelEnvironmentInfo::ICN_GROUND("icon_ground");
const std::string LLPanelEnvironmentInfo::CHK_ALLOWOVERRIDE("chk_allow_override");
const std::string LLPanelEnvironmentInfo::LBL_TIMEOFDAY("lbl_apparent_time");
const std::string LLPanelEnvironmentInfo::PNL_SETTINGS("pnl_environment_config");
const std::string LLPanelEnvironmentInfo::PNL_ENVIRONMENT_ALTITUDES("pnl_environment_altitudes");
const std::string LLPanelEnvironmentInfo::PNL_BUTTONS("pnl_environment_buttons");
const std::string LLPanelEnvironmentInfo::PNL_DISABLED("pnl_environment_disabled");
const std::string LLPanelEnvironmentInfo::TXT_DISABLED("txt_environment_disabled");
const std::string LLPanelEnvironmentInfo::SDT_DROP_TARGET("sdt_drop_target");

const std::string LLPanelEnvironmentInfo::STR_LABEL_USEDEFAULT("str_label_use_default");
const std::string LLPanelEnvironmentInfo::STR_LABEL_USEREGION("str_label_use_region");
const std::string LLPanelEnvironmentInfo::STR_LABEL_UNKNOWNINV("str_unknow_inventory");
const std::string LLPanelEnvironmentInfo::STR_ALTITUDE_DESCRIPTION("str_altitude_desription");
const std::string LLPanelEnvironmentInfo::STR_NO_PARCEL("str_no_parcel");
const std::string LLPanelEnvironmentInfo::STR_CROSS_REGION("str_cross_region");
const std::string LLPanelEnvironmentInfo::STR_LEGACY("str_legacy");
const std::string LLPanelEnvironmentInfo::STR_DISALLOWED("str_disallowed");

const U32 LLPanelEnvironmentInfo::DIRTY_FLAG_DAYCYCLE(0x01 << 0);
const U32 LLPanelEnvironmentInfo::DIRTY_FLAG_DAYLENGTH(0x01 << 1);
const U32 LLPanelEnvironmentInfo::DIRTY_FLAG_DAYOFFSET(0x01 << 2);
const U32 LLPanelEnvironmentInfo::DIRTY_FLAG_ALTITUDES(0x01 << 3);

const U32 LLPanelEnvironmentInfo::DIRTY_FLAG_MASK(
        LLPanelEnvironmentInfo::DIRTY_FLAG_DAYCYCLE | 
        LLPanelEnvironmentInfo::DIRTY_FLAG_DAYLENGTH | 
        LLPanelEnvironmentInfo::DIRTY_FLAG_DAYOFFSET |
        LLPanelEnvironmentInfo::DIRTY_FLAG_ALTITUDES);

const U32 ALTITUDE_SLIDER_COUNT = 3;

const std::string alt_sliders[] = {
    "sld1",
    "sld2",
    "sld3",
};

const std::string alt_prefixes[] = {
    "alt1",
    "alt2",
    "alt3",
    "ground",
    "water",
};

const std::string alt_panels[] = {
    "pnl_alt1",
    "pnl_alt2",
    "pnl_alt3",
    "pnl_ground",
    "pnl_water",
};

static LLDefaultChildRegistry::Register<LLSettingsDropTarget> r("settings_drop_target");

//=========================================================================
LLPanelEnvironmentInfo::LLPanelEnvironmentInfo(): 
    mCurrentEnvironment(),
    mDirtyFlag(0),
    mEditorLastParcelId(INVALID_PARCEL_ID),
    mCrossRegion(false),
    mNoSelection(false),
    mNoEnvironment(false),
    mCurEnvVersion(INVALID_PARCEL_ENVIRONMENT_VERSION),
    mSettingsFloater(),
    mEditFloater(),
    mAllowOverride(true)
{
}

LLPanelEnvironmentInfo::~LLPanelEnvironmentInfo()
{
    if (mChangeMonitor.connected())
        mChangeMonitor.disconnect();
    if (mCommitConnection.connected())
        mCommitConnection.disconnect();
    if (mUpdateConnection.connected())
        mUpdateConnection.disconnect();
}

BOOL LLPanelEnvironmentInfo::postBuild()
{

    getChild<LLUICtrl>(BTN_USEDEFAULT)->setCommitCallback([this](LLUICtrl *, const LLSD &){ onBtnDefault(); });
    getChild<LLUICtrl>(BTN_SELECTINV)->setCommitCallback([this](LLUICtrl *, const LLSD &){ onBtnSelect(); });
    getChild<LLUICtrl>(BTN_EDIT)->setCommitCallback([this](LLUICtrl *, const LLSD &){ onBtnEdit(); });

    getChild<LLUICtrl>(SLD_DAYLENGTH)->setCommitCallback([this](LLUICtrl *, const LLSD &value) { onSldDayLengthChanged(value.asReal()); });
    getChild<LLSliderCtrl>(SLD_DAYLENGTH)->setSliderMouseUpCallback([this](LLUICtrl *, const LLSD &) { onDayLenOffsetMouseUp(); });
    getChild<LLUICtrl>(SLD_DAYOFFSET)->setCommitCallback([this](LLUICtrl *, const LLSD &value) { onSldDayOffsetChanged(value.asReal()); });
    getChild<LLSliderCtrl>(SLD_DAYOFFSET)->setSliderMouseUpCallback([this](LLUICtrl *, const LLSD &) { onDayLenOffsetMouseUp(); });

    getChild<LLMultiSliderCtrl>(SLD_ALTITUDES)->setCommitCallback([this](LLUICtrl *cntrl, const LLSD &value) { onAltSliderCallback(cntrl, value); });
    getChild<LLMultiSliderCtrl>(SLD_ALTITUDES)->setSliderMouseUpCallback([this](LLUICtrl *, const LLSD &) { onAltSliderMouseUp(); });

    mChangeMonitor = LLEnvironment::instance().setEnvironmentChanged([this](LLEnvironment::EnvSelection_t env, S32 version) { onEnvironmentChanged(env, version); });

    // if we ever allow LLEstateInfoModel to work for non-EMs, uncomment this line.
    // mUpdateConnection = LLEstateInfoModel::instance().setUpdateCallback(boost::bind(&LLPanelEnvironmentInfo::refreshFromEstate, this));

    getChild<LLSettingsDropTarget>(SDT_DROP_TARGET)->setPanel(this);

    return TRUE;
}

// virtual
void LLPanelEnvironmentInfo::onOpen(const LLSD& key)
{
    refreshFromSource();
}

// virtual
void LLPanelEnvironmentInfo::onVisibilityChange(BOOL new_visibility)
{
    if (new_visibility)
    {
        gIdleCallbacks.addFunction(onIdlePlay, this);
    }
    else
    {
        LLFloaterSettingsPicker *picker = getSettingsPicker(false);
        if (picker)
        {
            picker->closeFloater();
        }

        gIdleCallbacks.deleteFunction(onIdlePlay, this);
        LLFloaterEditExtDayCycle *dayeditor = getEditFloater(false);
        if (mCommitConnection.connected())
            mCommitConnection.disconnect();

        if (dayeditor)
        {
            if (dayeditor->isDirty())
                dayeditor->refresh();
            else
            {
                dayeditor->closeFloater();
                mEditFloater.markDead();
            }
        }
    }

}

void LLPanelEnvironmentInfo::refresh()
{
    if (gDisconnected)
        return;

    if (!setControlsEnabled(canEdit()))
        return;

    if (!mCurrentEnvironment)
    {
        return;
    }

//     S32 rdo_selection = 0;
//     if ((!mCurrentEnvironment->mDayCycle) ||
//         ((mCurrentEnvironment->mParcelId == INVALID_PARCEL_ID) && (mCurrentEnvironment->mDayCycle->getAssetId() == LLSettingsDay::GetDefaultAssetId() )))
//     {
//         getChild<LLUICtrl>(EDT_INVNAME)->setValue("");
//     }
//     else if (!mCurrentEnvironment->mDayCycle->getAssetId().isNull())
//     {
//         rdo_selection = 1;
// 
//         LLUUID asset_id = mCurrentEnvironment->mDayCycle->getAssetId();
// 
//         std::string inventoryname = getInventoryNameForAssetId(asset_id);
// 
//         if (inventoryname.empty())
//             inventoryname = "(" + mCurrentEnvironment->mDayCycle->getName() + ")";
// 
//         getChild<LLUICtrl>(EDT_INVNAME)->setValue(inventoryname);
//     }
//     else
//     {   // asset id is null so this is a custom environment
//         rdo_selection = 2;
//         getChild<LLUICtrl>(EDT_INVNAME)->setValue("");
//     }
//     getChild<LLRadioGroup>(RDG_ENVIRONMENT_SELECT)->setSelectedIndex(rdo_selection);

    F32Hours daylength(mCurrentEnvironment->mDayLength);
    F32Hours dayoffset(mCurrentEnvironment->mDayOffset);

    if (dayoffset.value() > 12.0f)
        dayoffset -= F32Hours(24.0);

    getChild<LLSliderCtrl>(SLD_DAYLENGTH)->setValue(daylength.value());
    getChild<LLSliderCtrl>(SLD_DAYOFFSET)->setValue(dayoffset.value());
//     getChild<LLSliderCtrl>(SLD_DAYLENGTH)->setEnabled(canEdit() && (rdo_selection != 0) && !mCurrentEnvironment->mIsLegacy);
//     getChild<LLSliderCtrl>(SLD_DAYOFFSET)->setEnabled(canEdit() && (rdo_selection != 0) && !mCurrentEnvironment->mIsLegacy);
   
    udpateApparentTimeOfDay();

    updateEditFloater(mCurrentEnvironment, canEdit());

    LLEnvironment::altitude_list_t altitudes = LLEnvironment::instance().getRegionAltitudes();
    if (altitudes.size() > 0)
    {
        LLMultiSliderCtrl *sld = getChild<LLMultiSliderCtrl>(SLD_ALTITUDES);
        sld->clear();

        for (S32 idx = 0; idx < ALTITUDE_SLIDER_COUNT; ++idx)
        {
            sld->addSlider(altitudes[idx + 1], alt_sliders[idx]);
            updateAltLabel(alt_prefixes[idx], idx + 2, altitudes[idx + 1]);
            mAltitudes[alt_sliders[idx]] = AltitudeData(idx+1, idx, altitudes[idx+1]);
        }
        if (sld->getCurNumSliders() != ALTITUDE_SLIDER_COUNT)
        {
            LL_WARNS("ENVPANEL") << "Failed to add altitude sliders!" << LL_ENDL;
        }
        readjustAltLabels();
    }

    updateAltLabel(alt_prefixes[3], 1, 0); // ground
    updateAltLabel(alt_prefixes[4], 0, 0); // water

}

void LLPanelEnvironmentInfo::refreshFromEstate()
{
    /*TODO: Unfortunately only estate manager may get information from the LLEstateInfoModel.  
     * The proletariat is not allowed to know what options are set for an estate. We should fix this.*/
    LLViewerRegion *pRegion = gAgent.getRegion();

    bool oldAO = mAllowOverride;
    //mAllowOverride = (!isRegion()) || LLEstateInfoModel::instance().getAllowEnvironmentOverride();
    mAllowOverride = (isRegion() && LLEstateInfoModel::instance().getAllowEnvironmentOverride()) || pRegion->getAllowEnvironmentOverride();
    if (oldAO != mAllowOverride)
        refresh();
}

std::string LLPanelEnvironmentInfo::getInventoryNameForAssetId(LLUUID asset_id) 
{
    std::string name(LLFloaterSettingsPicker::findItemName(asset_id, false, false));

    if (name.empty())
        return getString(STR_LABEL_UNKNOWNINV);
    return name;
}


std::string LLPanelEnvironmentInfo::getNameForTrackIndex(S32 index)
{
    std::string invname;

    LL_WARNS("LAPRAS") << "mDayCycleName='" << mCurrentEnvironment->mDayCycleName << "'" << LL_ENDL;
    if (mCurrentEnvironment->mDayCycleName.empty())
    {
        invname = mCurrentEnvironment->mNameList[index];
    }
    else if (!mCurrentEnvironment->mDayCycle->isTrackEmpty(index))
    {
        invname = mCurrentEnvironment->mDayCycleName;
    }

    if (invname.empty())
        invname = getString("str_empty");

    return invname;
}

LLFloaterSettingsPicker * LLPanelEnvironmentInfo::getSettingsPicker(bool create)
{
    LLFloaterSettingsPicker *picker = static_cast<LLFloaterSettingsPicker *>(mSettingsFloater.get());

    // Show the dialog
    if (!picker && create)
    {
        picker = new LLFloaterSettingsPicker(this,
            LLUUID::null, "SELECT SETTINGS");

        mSettingsFloater = picker->getHandle();

        picker->setCommitCallback([this](LLUICtrl *, const LLSD &data){ onPickerCommitted(data.asUUID()); });
    }

    return picker;
}

LLFloaterEditExtDayCycle * LLPanelEnvironmentInfo::getEditFloater(bool create)
{
    static const S32 FOURHOURS(4 * 60 * 60);
    LLFloaterEditExtDayCycle *editor = static_cast<LLFloaterEditExtDayCycle *>(mEditFloater.get());

    // Show the dialog
    if (!editor && create)
    {
        LLSD params(LLSDMap(LLFloaterEditExtDayCycle::KEY_EDIT_CONTEXT, isRegion() ? LLFloaterEditExtDayCycle::CONTEXT_REGION : LLFloaterEditExtDayCycle::CONTEXT_PARCEL)
            (LLFloaterEditExtDayCycle::KEY_DAY_LENGTH, mCurrentEnvironment ? (S32)(mCurrentEnvironment->mDayLength.value()) : FOURHOURS));

        editor = (LLFloaterEditExtDayCycle *)LLFloaterReg::getInstance(FLOATER_DAY_CYCLE_EDIT, params);

        if (!editor)
            return nullptr;
        mEditFloater = editor->getHandle();
    }

    if (editor && !mCommitConnection.connected())
        mCommitConnection = editor->setEditCommitSignal([this](LLSettingsDay::ptr_t pday) { onEditCommitted(pday); });

    return editor;
}


void LLPanelEnvironmentInfo::updateEditFloater(const LLEnvironment::EnvironmentInfo::ptr_t &nextenv, bool enable)
{
    LLFloaterEditExtDayCycle *dayeditor(getEditFloater(false));

    if (!dayeditor)
        return;

    if (!nextenv || !nextenv->mDayCycle || !enable)
    {
        if (mCommitConnection.connected())
            mCommitConnection.disconnect();

        if (dayeditor->isDirty())
            dayeditor->refresh();
        else
            dayeditor->closeFloater();
    }
    else if (dayeditor->getEditingAssetId() != nextenv->mDayCycle->getAssetId()
            || mEditorLastParcelId != nextenv->mParcelId
            || mEditorLastRegionId != nextenv->mRegionId)
    {
        // Ignore dirty
        // If parcel selection changed whatever we do except saving to inventory with
        // old settings will be invalid.
        mEditorLastParcelId = nextenv->mParcelId;
        mEditorLastRegionId = nextenv->mRegionId;
        dayeditor->setEditDayCycle(nextenv->mDayCycle);
    }
}

bool LLPanelEnvironmentInfo::setControlsEnabled(bool enabled)
{
    bool is_unavailable(false);
    bool is_legacy = (mCurrentEnvironment) ? mCurrentEnvironment->mIsLegacy : true;

    if (mNoEnvironment || (!LLEnvironment::instance().isExtendedEnvironmentEnabled() && !isRegion()))
    {
        is_unavailable = true;
        getChild<LLTextBox>(TXT_DISABLED)->setText(getString(STR_LEGACY));
    }
    else if (mNoSelection)
    {
        is_unavailable = true;
        getChild<LLTextBox>(TXT_DISABLED)->setText(getString(STR_NO_PARCEL));
    }
    else if (mCrossRegion)
    {
        is_unavailable = true;
        getChild<LLTextBox>(TXT_DISABLED)->setText(getString(STR_CROSS_REGION));
    }
    else if (!isRegion() && !mAllowOverride)
    {
        is_unavailable = true;
        getChild<LLTextBox>(TXT_DISABLED)->setText(getString(STR_DISALLOWED));
    }

    if (is_unavailable)
    {
        getChild<LLUICtrl>(PNL_SETTINGS)->setVisible(false);
        getChild<LLUICtrl>(PNL_BUTTONS)->setVisible(false);
        getChild<LLUICtrl>(PNL_DISABLED)->setVisible(true);
        getChild<LLUICtrl>(PNL_ENVIRONMENT_ALTITUDES)->setVisible(FALSE);

        updateEditFloater(mCurrentEnvironment, false);

        return false;
    }
    getChild<LLUICtrl>(PNL_SETTINGS)->setVisible(true);
    getChild<LLUICtrl>(PNL_BUTTONS)->setVisible(true);
    getChild<LLUICtrl>(PNL_DISABLED)->setVisible(false);

    getChild<LLUICtrl>(PNL_ENVIRONMENT_ALTITUDES)->setVisible(LLEnvironment::instance().isExtendedEnvironmentEnabled());

    bool can_enable = enabled && mCurrentEnvironment && (mCurEnvVersion != INVALID_PARCEL_ENVIRONMENT_VERSION);
    getChild<LLUICtrl>(BTN_SELECTINV)->setEnabled(can_enable && !is_legacy);
    getChild<LLUICtrl>(BTN_USEDEFAULT)->setEnabled(can_enable && !is_legacy);
    getChild<LLUICtrl>(BTN_EDIT)->setEnabled(can_enable);
    getChild<LLUICtrl>(SLD_DAYLENGTH)->setEnabled(can_enable && !is_legacy);
    getChild<LLUICtrl>(SLD_DAYOFFSET)->setEnabled(can_enable && !is_legacy);
    getChild<LLUICtrl>(SLD_ALTITUDES)->setEnabled(can_enable && isRegion() && !is_legacy);
    getChild<LLUICtrl>(ICN_GROUND)->setColor((can_enable && isRegion() && !is_legacy) ? LLColor4::white : LLColor4::grey % 0.8f);
    getChild<LLUICtrl>(PNL_ENVIRONMENT_ALTITUDES)->setEnabled(can_enable && isRegion() && !is_legacy);
    getChild<LLUICtrl>(CHK_ALLOWOVERRIDE)->setEnabled(can_enable && isRegion() && !is_legacy);

    getChild<LLSettingsDropTarget>(SDT_DROP_TARGET)->setDndEnabled(enabled && !is_legacy);

    return true;
}

void LLPanelEnvironmentInfo::setApplyProgress(bool started)
{
//     LLLoadingIndicator* indicator = getChild<LLLoadingIndicator>("progress_indicator");
// 
//     indicator->setVisible(started);
// 
//     if (started)
//     {
//         indicator->start();
//     }
//     else
//     {
//         indicator->stop();
//     }
}

void LLPanelEnvironmentInfo::setDirtyFlag(U32 flag)
{
    mDirtyFlag |= flag;
}

void LLPanelEnvironmentInfo::clearDirtyFlag(U32 flag)
{
    mDirtyFlag &= ~flag;
}

void LLPanelEnvironmentInfo::updateAltLabel(const std::string &alt_prefix, U32 sky_index, F32 alt_value)
{
    LLMultiSliderCtrl *sld = findChild<LLMultiSliderCtrl>(SLD_ALTITUDES);
    if (!sld)
    {
        LL_WARNS() << "Failed to find slider " << SLD_ALTITUDES << LL_ENDL;
        return;
    }
    LLRect sld_rect = sld->getRect();
    S32 sld_range = sld_rect.getHeight();
    S32 sld_bottom = sld_rect.mBottom;
    S32 sld_offset = sld_rect.getWidth(); // Roughly identical to thumb's width in slider.
    S32 pos = (sld_range - sld_offset) * ((alt_value - 100) / (4000 - 100));

    // get related views
    LLTextBox* text = findChild<LLTextBox>("txt_" + alt_prefix);
    LLLineEditor *field = findChild<LLLineEditor>("edt_invname_" + alt_prefix);
    LLView *alt_panel = findChild<LLView>("pnl_" + alt_prefix);

    if (text && (sky_index > 1))
    {
        // update text
        std::ostringstream convert;
        convert << alt_value;
        text->setTextArg("[ALTITUDE]", convert.str());
        convert.str("");
        convert.clear();
        convert << sky_index;
        text->setTextArg("[INDEX]", convert.str());
    }

    if (field)
    {
        field->setText(getNameForTrackIndex(sky_index));
    }

    if (alt_panel && (sky_index > 1))
    {
        // move containing panel
        LLRect rect = alt_panel->getRect();
        S32 height = rect.getHeight();
        rect.mBottom = sld_bottom + (sld_offset / 2 + 1) + pos - (height / 2);
        rect.mTop = rect.mBottom + height;
        alt_panel->setRect(rect);
    }

}

void LLPanelEnvironmentInfo::readjustAltLabels()
{
    // Re-adjust all labels
    // Very simple "adjust after the fact" method
    // Note: labels can be in any ordered
    for (U32 i = 0; i < ALTITUDE_SLIDER_COUNT - 1; i++)
    {
        LLView* view_cmp = findChild<LLView>(alt_panels[i]);

        for (U32 j = i + 1; j < ALTITUDE_SLIDER_COUNT; j++)
        {
            LLView* view_intr = findChild<LLView>(alt_panels[j]);
            if (view_cmp && view_intr)
            {
                LLRect cmp_rect = view_cmp->getRect();
                LLRect intr_rect = view_intr->getRect();
                S32 shift = 0;
                if (cmp_rect.mBottom <= intr_rect.mTop && cmp_rect.mBottom >= intr_rect.mBottom)
                {
                    // Approximate shift
                    // We probably will need more cycle runs over all labels to get accurate one
                    // At the moment single cycle should do since we have too little elements to do something complicated
                    shift = (cmp_rect.mBottom - intr_rect.mTop) / 2;
                }
                else if (cmp_rect.mTop >= intr_rect.mBottom && cmp_rect.mTop <= intr_rect.mTop)
                {
                    // Approximate shift
                    shift = (cmp_rect.mTop - intr_rect.mBottom) / 2;
                }
                if (shift != 0)
                {
                    cmp_rect.translate(0, -shift);
                    view_cmp->setRect(cmp_rect);

                    intr_rect.translate(0, shift);
                    view_intr->setRect(intr_rect);
                }
            }
        }
    }
}

void LLPanelEnvironmentInfo::onSldDayLengthChanged(F32 value)
{
    F32Hours daylength(value);

    mCurrentEnvironment->mDayLength = daylength;
    setDirtyFlag(DIRTY_FLAG_DAYLENGTH);

    udpateApparentTimeOfDay();
}

void LLPanelEnvironmentInfo::onSldDayOffsetChanged(F32 value)
{
    F32Hours dayoffset(value);

    if (dayoffset.value() <= 0.0f)
        dayoffset += F32Hours(24.0);

    mCurrentEnvironment->mDayOffset = dayoffset;
    setDirtyFlag(DIRTY_FLAG_DAYOFFSET);

    udpateApparentTimeOfDay();
}

void LLPanelEnvironmentInfo::onDayLenOffsetMouseUp()
{
    if (getDirtyFlag() & (DIRTY_FLAG_DAYLENGTH | DIRTY_FLAG_DAYOFFSET))
    {
        clearDirtyFlag(DIRTY_FLAG_DAYOFFSET);
        clearDirtyFlag(DIRTY_FLAG_DAYLENGTH);

        LLHandle<LLPanel> that_h = getHandle();

        LLEnvironment::instance().updateParcel(getParcelId(), LLSettingsDay::ptr_t(),
            mCurrentEnvironment->mDayLength.value(), mCurrentEnvironment->mDayOffset.value(), LLEnvironment::altitudes_vect_t(),
            [that_h](S32 parcel_id, LLEnvironment::EnvironmentInfo::ptr_t envifo) { _onEnvironmentReceived(that_h, parcel_id, envifo); });

    }
}

void LLPanelEnvironmentInfo::onAltSliderCallback(LLUICtrl *cntrl, const LLSD &data)
{
    LLMultiSliderCtrl *sld = (LLMultiSliderCtrl *)cntrl;
    std::string sld_name = sld->getCurSlider();
    F32 sld_value = sld->getCurSliderValue();

    mAltitudes[sld_name].mAltitude = sld_value;

    // update all labels since we could have jumped multiple and we will need to readjust
    // (or sort by altitude, too little elements, so I didn't bother with efficiency)
    altitudes_data_t::iterator end = mAltitudes.end();
    altitudes_data_t::iterator iter = mAltitudes.begin();
    altitudes_data_t::iterator iter2;
    U32 new_index;
    while (iter != end)
    {
        iter2 = mAltitudes.begin();
        new_index = 1;
        while (iter2 != end)
        {
            if (iter->second.mAltitude > iter2->second.mAltitude)
            {
                new_index++;
            }
            iter2++;
        }
        iter->second.mAltitudeIndex = new_index;

        updateAltLabel(alt_prefixes[iter->second.mLabelIndex], iter->second.mAltitudeIndex + 1, iter->second.mAltitude);
        iter++;
    }

    readjustAltLabels();
    setDirtyFlag(DIRTY_FLAG_ALTITUDES);
}

void LLPanelEnvironmentInfo::onAltSliderMouseUp()
{
    if (isRegion() && (getDirtyFlag() & DIRTY_FLAG_ALTITUDES))
    {
        clearDirtyFlag(DIRTY_FLAG_ALTITUDES);

        LLHandle<LLPanel> that_h = getHandle();
        LLEnvironment::altitudes_vect_t alts;

        for (auto alt : mAltitudes)
        {
            alts.push_back(alt.second.mAltitude);
        }

        LLEnvironment::instance().updateParcel(getParcelId(), LLSettingsDay::ptr_t(),
            -1, -1, alts,
            [that_h](S32 parcel_id, LLEnvironment::EnvironmentInfo::ptr_t envifo) { _onEnvironmentReceived(that_h, parcel_id, envifo); });

    }
}

void LLPanelEnvironmentInfo::onBtnDefault()
{
    LLHandle<LLPanel> that_h = getHandle();

    LLEnvironment::instance().resetParcel(getParcelId(),
        [that_h](S32 parcel_id, LLEnvironment::EnvironmentInfo::ptr_t envifo) { _onEnvironmentReceived(that_h, parcel_id, envifo); });
}

void LLPanelEnvironmentInfo::onBtnEdit()
{
    static const S32 FOURHOURS(4 * 60 * 60);

    LLFloaterEditExtDayCycle *dayeditor = getEditFloater();

    LLSD params(LLSDMap(LLFloaterEditExtDayCycle::KEY_EDIT_CONTEXT, isRegion() ? LLFloaterEditExtDayCycle::VALUE_CONTEXT_REGION : LLFloaterEditExtDayCycle::VALUE_CONTEXT_PARCEL)
            (LLFloaterEditExtDayCycle::KEY_DAY_LENGTH,  mCurrentEnvironment ? (S32)(mCurrentEnvironment->mDayLength.value()) : FOURHOURS)
            (LLFloaterEditExtDayCycle::KEY_CANMOD,      LLSD::Boolean(true)));

    dayeditor->openFloater(params);
    if (mCurrentEnvironment && mCurrentEnvironment->mDayCycle)
    {
        dayeditor->setEditDayCycle(mCurrentEnvironment->mDayCycle);
        if (!ends_with(mCurrentEnvironment->mDayCycle->getName(), "(customized)"))
        {
            dayeditor->setEditName(mCurrentEnvironment->mDayCycle->getName() + "(customized)");
        }
    }
    else
        dayeditor->setEditDefaultDayCycle();
}

void LLPanelEnvironmentInfo::onBtnSelect()
{
    LLFloaterSettingsPicker *picker = getSettingsPicker();
    if (picker)
    {
        LLUUID item_id;
        if (mCurrentEnvironment && mCurrentEnvironment->mDayCycle)
        {
            item_id = LLFloaterSettingsPicker::findItemID(mCurrentEnvironment->mDayCycle->getAssetId(), false, false);
        }
        picker->setSettingsFilter(LLSettingsType::ST_NONE);
        picker->setSettingsItemId(item_id);
        picker->openFloater();
        picker->setFocus(TRUE);
    }
}


void LLPanelEnvironmentInfo::udpateApparentTimeOfDay()
{
    static const F32 SECONDSINDAY(24.0 * 60.0 * 60.0);

    if ((!mCurrentEnvironment) || (mCurrentEnvironment->mDayLength.value() < 1.0) || (mCurrentEnvironment->mDayOffset.value() < 1.0))
    {
        getChild<LLUICtrl>(LBL_TIMEOFDAY)->setVisible(false);
        return;
    }
    getChild<LLUICtrl>(LBL_TIMEOFDAY)->setVisible(true);

    S32Seconds  now(LLDate::now().secondsSinceEpoch());

    now += mCurrentEnvironment->mDayOffset;

    F32 perc = (F32)(now.value() % mCurrentEnvironment->mDayLength.value()) / (F32)(mCurrentEnvironment->mDayLength.value());

    S32Seconds  secondofday((S32)(perc * SECONDSINDAY));
    S32Hours    hourofday(secondofday);
    S32Seconds  secondofhour(secondofday - hourofday);
    S32Minutes  minutesofhour(secondofhour);
    bool        am_pm(hourofday.value() >= 12);

    if (hourofday.value() < 1)
        hourofday = S32Hours(12);
    if (hourofday.value() > 12)
        hourofday -= S32Hours(12);

    std::string lblminute(((minutesofhour.value() < 10) ? "0" : "") + LLSD(minutesofhour.value()).asString());


    getChild<LLUICtrl>(LBL_TIMEOFDAY)->setTextArg("[HH]", LLSD(hourofday.value()).asString());
    getChild<LLUICtrl>(LBL_TIMEOFDAY)->setTextArg("[MM]", lblminute);
    getChild<LLUICtrl>(LBL_TIMEOFDAY)->setTextArg("[AP]", std::string(am_pm ? "PM" : "AM"));
    getChild<LLUICtrl>(LBL_TIMEOFDAY)->setTextArg("[PRC]", LLSD((S32)(100 * perc)).asString());

}

void LLPanelEnvironmentInfo::onIdlePlay(void *data)
{
    ((LLPanelEnvironmentInfo *)data)->udpateApparentTimeOfDay();
}

void LLPanelEnvironmentInfo::onPickerCommitted(LLUUID item_id)
{
    LLInventoryItem *itemp = gInventory.getItem(item_id);
    if (itemp)
    {
        LLHandle<LLPanel> that_h = getHandle();

        LLEnvironment::instance().updateParcel(getParcelId(), itemp->getAssetUUID(),
            itemp->getName(), 
            -1, -1, LLEnvironment::altitudes_vect_t(),
            [that_h](S32 parcel_id, LLEnvironment::EnvironmentInfo::ptr_t envifo) { _onEnvironmentReceived(that_h, parcel_id, envifo); });
    }
}

void LLPanelEnvironmentInfo::onEditCommitted(LLSettingsDay::ptr_t newday)
{
    LLEnvironment::instance().clearEnvironment(LLEnvironment::ENV_EDIT);
    LLEnvironment::instance().updateEnvironment();
    if (!newday)
    {
        LL_WARNS("ENVPANEL") << "Editor committed an empty day. Do nothing." << LL_ENDL;
        return;
    }
    if (!mCurrentEnvironment)
    {
        // Attempting to save mid update?
        LL_WARNS("ENVPANEL") << "Failed to apply changes from editor! Dirty state: " << mDirtyFlag << " env version: " << mCurEnvVersion << LL_ENDL;
        return;
    }
    size_t newhash(newday->getHash());
    size_t oldhash((mCurrentEnvironment->mDayCycle) ? mCurrentEnvironment->mDayCycle->getHash() : 0);

    if (newhash != oldhash)
    {
        LLHandle<LLPanel> that_h = getHandle();

        LLEnvironment::instance().updateParcel(getParcelId(), newday,
            -1, -1, LLEnvironment::altitudes_vect_t(),
            [that_h](S32 parcel_id, LLEnvironment::EnvironmentInfo::ptr_t envifo) { _onEnvironmentReceived(that_h, parcel_id, envifo); });
    }
}

void LLPanelEnvironmentInfo::onEnvironmentChanged(LLEnvironment::EnvSelection_t env, S32 new_version)
{
    if (new_version < INVALID_PARCEL_ENVIRONMENT_VERSION)
    {
        // cleanups and local changes, we are only interested in changes sent by server
        return;
    }

    LL_DEBUGS("ENVPANEL") << "Received environment update " << mCurEnvVersion << " " << new_version << LL_ENDL;

    // Environment comes from different sources, from environment update callbacks,
    // from hovers (causes callbacks on version change) and from personal requests
    // filter out duplicates and out of order packets by checking parcel environment version.

    if (isRegion())
    {
        // Note: region uses same init versions as parcel
        if (env == LLEnvironment::ENV_REGION
            // version should be always growing, UNSET_PARCEL_ENVIRONMENT_VERSION is backup case
            && (mCurEnvVersion < new_version || mCurEnvVersion <= UNSET_PARCEL_ENVIRONMENT_VERSION))
        {
            if (new_version >= UNSET_PARCEL_ENVIRONMENT_VERSION)
            {
                // 'pending state' to prevent re-request on following onEnvironmentChanged if there will be any
                mCurEnvVersion = new_version;
            }
            mCurrentEnvironment.reset();
            refreshFromSource();
        }
    }
    else if ((env == LLEnvironment::ENV_PARCEL)
             && (getParcelId() == LLViewerParcelMgr::instance().getAgentParcelId()))
    {
        LLParcel *parcel = getParcel();
        if (parcel)
        {
            // first for parcel own settings, second is for case when parcel uses region settings
            if (mCurEnvVersion < new_version
                || (mCurEnvVersion != new_version && new_version == UNSET_PARCEL_ENVIRONMENT_VERSION))
            {
                // 'pending state' to prevent re-request on following onEnvironmentChanged if there will be any
                mCurEnvVersion = new_version;
                mCurrentEnvironment.reset();

                refreshFromSource();
            }
            else if (mCurrentEnvironment)
            {
                // update controls
                refresh();
            }
        }
    }
}


void LLPanelEnvironmentInfo::onPickerAssetDownloaded(LLSettingsBase::ptr_t settings)
{
    LLSettingsVODay::buildFromOtherSetting(settings, [this](LLSettingsDay::ptr_t pday)
        {
            if (pday)
            {
                mCurrentEnvironment->mDayCycle = pday;
                setDirtyFlag(DIRTY_FLAG_DAYCYCLE);
            }
            refresh();
        });
}

void LLPanelEnvironmentInfo::onEnvironmentReceived(S32 parcel_id, LLEnvironment::EnvironmentInfo::ptr_t envifo)
{
    if (parcel_id != getParcelId())
    {
        LL_WARNS("ENVPANEL") << "Have environment for parcel " << parcel_id << " expecting " << getParcelId() << ". Discarding." << LL_ENDL;
        return;
    }
    mCurrentEnvironment = envifo;
    clearDirtyFlag(DIRTY_FLAG_MASK);
    if (mCurrentEnvironment->mEnvVersion > INVALID_PARCEL_ENVIRONMENT_VERSION)
    {
        // Server provided version, use it
        mCurEnvVersion = mCurrentEnvironment->mEnvVersion;
        LL_DEBUGS("ENVPANEL") << " Setting environment version: " << mCurEnvVersion << " for parcel id: " << parcel_id << LL_ENDL;
    }
    // Backup: Version was not provided for some reason
    else
    {
        LL_WARNS("ENVPANEL") << " Environment version was not provided for " << parcel_id << ", old env version: " << mCurEnvVersion << LL_ENDL;
    }

    refreshFromEstate();
    refresh();

    // todo: we have envifo and parcel env version, should we just setEnvironment() and parcel's property to prevent dupplicate requests?
}

void LLPanelEnvironmentInfo::_onEnvironmentReceived(LLHandle<LLPanel> that_h, S32 parcel_id, LLEnvironment::EnvironmentInfo::ptr_t envifo)
{
    LLPanelEnvironmentInfo *that = (LLPanelEnvironmentInfo *)that_h.get();
    if (!that)
        return;
    that->onEnvironmentReceived(parcel_id, envifo);
}

LLSettingsDropTarget::LLSettingsDropTarget(const LLSettingsDropTarget::Params& p)
    : LLView(p), mEnvironmentInfoPanel(NULL), mDndEnabled(false)
{}

BOOL LLSettingsDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
	EDragAndDropType cargo_type,
	void* cargo_data,
	EAcceptance* accept,
	std::string& tooltip_msg)
{
    BOOL handled = FALSE;

    if (getParent() && mDndEnabled)
    {
        handled = TRUE;

        switch (cargo_type)
        {
        case DAD_SETTINGS:
        {
            LLViewerInventoryItem* inv_item = (LLViewerInventoryItem*)cargo_data;
            if (inv_item && mEnvironmentInfoPanel)
            {
                LLUUID item_id = inv_item->getUUID();
                if (gInventory.getItem(item_id))
                {
                    *accept = ACCEPT_YES_COPY_SINGLE;
                    if (drop)
                    {
                        mEnvironmentInfoPanel->onPickerCommitted(item_id);
                    }
                }
            }
            else
            {
                *accept = ACCEPT_NO;
            }
            break;
        }
        default:
            *accept = ACCEPT_NO;
            break;
        }
    }
    return handled;
}
