//================ Copyright (c) 2015, PG, All rights reserved. =================//
//
// Purpose:		base class for all gameplay objects
//
// $NoKeywords: $hitobj
//===============================================================================//

#include "OsuHitObject.h"

#include "Engine.h"
#include "ResourceManager.h"
#include "AnimationHandler.h"
#include "ConVar.h"

#include "Osu.h"
#include "OsuSkin.h"
#include "OsuSkinImage.h"
#include "OsuGameRules.h"
#include "OsuBeatmapStandard.h"
#include "OsuHUD.h"

ConVar osu_hitresult_draw_300s("osu_hitresult_draw_300s", false);

ConVar osu_hitresult_scale("osu_hitresult_scale", 1.0f);
ConVar osu_hitresult_duration("osu_hitresult_duration", 1.25f);

ConVar osu_approach_scale_multiplier("osu_approach_scale_multiplier", 3.0f);

ConVar osu_mod_target_300_percent("osu_mod_target_300_percent", 0.5f);
ConVar osu_mod_target_100_percent("osu_mod_target_100_percent", 0.7f);
ConVar osu_mod_target_50_percent("osu_mod_target_50_percent", 0.95f);

ConVar *OsuHitObject::m_osu_approach_scale_multiplier_ref = &osu_approach_scale_multiplier;
unsigned long long OsuHitObject::sortHackCounter = 0;

void OsuHitObject::drawHitResult(Graphics *g, OsuBeatmapStandard *beatmap, Vector2 rawPos, OsuScore::HIT result, float animPercent, float defaultAnimPercent)
{
	drawHitResult(g, beatmap->getSkin(), beatmap->getHitcircleDiameter(), beatmap->getRawHitcircleDiameter(), rawPos, result, animPercent, defaultAnimPercent);
}

void OsuHitObject::drawHitResult(Graphics *g, OsuSkin *skin, float hitcircleDiameter, float rawHitcircleDiameter, Vector2 rawPos, OsuScore::HIT result, float animPercent, float defaultAnimPercent)
{
	if (animPercent <= 0.0f) return;

	const float osuCoordScaleMultiplier = hitcircleDiameter / rawHitcircleDiameter;

	g->setColor(0xffffffff);
	g->setAlpha(animPercent);
	g->pushTransform();
		float hitImageScale = 1.0f;
		switch (result)
		{
		case OsuScore::HIT::HIT_MISS:
			hitImageScale = (rawHitcircleDiameter / skin->getHit0()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
			break;
		case OsuScore::HIT::HIT_50:
			hitImageScale = (rawHitcircleDiameter / skin->getHit50()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
			break;
		case OsuScore::HIT::HIT_100:
			hitImageScale = (rawHitcircleDiameter / skin->getHit100()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
			break;
		case OsuScore::HIT::HIT_300:
			hitImageScale = (rawHitcircleDiameter / skin->getHit300()->getSizeBaseRaw().x) * osuCoordScaleMultiplier;
			break;
		}
		switch (result)
		{
		case OsuScore::HIT::HIT_MISS:
			g->translate(0, (1.0f-defaultAnimPercent)*(1.0f-defaultAnimPercent)*(1.0f-defaultAnimPercent)*skin->getHit0()->getSize().y*1.25f);
			/*
			g->pushTransform();
			g->translate(rawPos.x, rawPos.y);
			g->setColor(0xffffffff);
			g->drawString(engine->getResourceManager()->getFont("FONT_DEFAULT"), UString::format("animPercent = %f, frame = %i\n", animPercent, skin->getHit0()->getFrameNumber()));
			g->popTransform();
			*/
			if (skin->getHit0()->getNumImages() > 1) // special case: animated hitresults don't fade out
				g->setAlpha(1.0f);
			skin->getHit0()->drawRaw(g, rawPos, hitImageScale*osu_hitresult_scale.getFloat());
			break;
		case OsuScore::HIT::HIT_50:
			if (skin->getHit50()->getNumImages() > 1) // special case: animated hitresults don't fade out
				g->setAlpha(1.0f);
			skin->getHit50()->drawRaw(g, rawPos, hitImageScale*osu_hitresult_scale.getFloat());
			break;
		case OsuScore::HIT::HIT_100:
			if (skin->getHit100()->getNumImages() > 1) // special case: animated hitresults don't fade out
				g->setAlpha(1.0f);
			skin->getHit100()->drawRaw(g, rawPos, hitImageScale*osu_hitresult_scale.getFloat());
			break;
		case OsuScore::HIT::HIT_300:
			if (osu_hitresult_draw_300s.getBool())
			{
				if (skin->getHit300()->getNumImages() > 1) // special case: animated hitresults don't fade out
					g->setAlpha(1.0f);
				skin->getHit300()->drawRaw(g, rawPos, hitImageScale*osu_hitresult_scale.getFloat());
			}
			break;
		}
	g->popTransform();
}



OsuHitObject::OsuHitObject(long time, int sampleType, int comboNumber, int colorCounter, OsuBeatmap *beatmap)
{
	m_iTime = time;
	m_iSampleType = sampleType;
	m_iComboNumber = comboNumber;
	m_iColorCounter = colorCounter;
	m_beatmap = beatmap;

	m_fAlpha = 0.0f;
	m_fApproachScale = 0.0f;
	m_fFadeInScale = 0.0f;
	m_iApproachTime = 0;
	m_iFadeInTime = 0;
	m_iObjectTime = 0;
	m_iObjectDuration = 0;
	m_iDelta = 0;
	m_iObjectTime = 0;
	m_iObjectDuration = 0;

	m_iHiddenDecayTime = 0;
	m_iHiddenTimeDiff = 0;

	m_bVisible = false;
	m_bFinished = false;
	m_bBlocked = false;
	m_bMisAim = false;
	m_iAutopilotDelta = 0;
	m_bOverrideHDApproachCircle = false;

	m_iStack = 0;

	m_iSortHack = sortHackCounter++;
}

void OsuHitObject::draw(Graphics *g)
{
	if (m_hitResults.size() > 0)
	{
		OsuBeatmapStandard *beatmapStd = dynamic_cast<OsuBeatmapStandard*>(m_beatmap);
		if (beatmapStd != NULL)
		{
			for (int i=0; i<m_hitResults.size(); i++)
			{
				const long offset = m_iTime + m_iObjectDuration + m_hitResults[i].delta;

				beatmapStd->getSkin()->getHit0()->setAnimationTimeOffset(offset);
				beatmapStd->getSkin()->getHit0()->setAnimationFrameClampUp();
				beatmapStd->getSkin()->getHit50()->setAnimationTimeOffset(offset);
				beatmapStd->getSkin()->getHit50()->setAnimationFrameClampUp();
				beatmapStd->getSkin()->getHit100()->setAnimationTimeOffset(offset);
				beatmapStd->getSkin()->getHit100()->setAnimationFrameClampUp();
				beatmapStd->getSkin()->getHit100k()->setAnimationTimeOffset(offset);
				beatmapStd->getSkin()->getHit100k()->setAnimationFrameClampUp();
				beatmapStd->getSkin()->getHit300()->setAnimationTimeOffset(offset);
				beatmapStd->getSkin()->getHit300()->setAnimationFrameClampUp();
				beatmapStd->getSkin()->getHit300g()->setAnimationTimeOffset(offset);
				beatmapStd->getSkin()->getHit300g()->setAnimationFrameClampUp();
				beatmapStd->getSkin()->getHit300k()->setAnimationTimeOffset(offset);
				beatmapStd->getSkin()->getHit300k()->setAnimationFrameClampUp();

				drawHitResult(g, beatmapStd, beatmapStd->osuCoords2Pixels(m_hitResults[i].rawPos), m_hitResults[i].result, clamp<float>(((m_hitResults[i].anim - engine->getTime()) / m_hitResults[i].duration), 0.0f, 1.0f), ((m_hitResults[i].defaultanim - engine->getTime()) / m_hitResults[i].defaultduration));
			}
		}
	}
}

void OsuHitObject::update(long curPos)
{
	m_iApproachTime = (long)OsuGameRules::getApproachTime(m_beatmap);
	m_iHiddenDecayTime = OsuGameRules::getHiddenDecayTimeFromApproachTime(m_iApproachTime);
	m_iHiddenTimeDiff = OsuGameRules::getHiddenTimeDiffFromApproachTime(m_iApproachTime);
	m_iFadeInTime = OsuGameRules::getFadeInTimeFromApproachTime(m_iApproachTime);

	m_iObjectTime = m_iApproachTime + m_iFadeInTime + (m_beatmap->getOsu()->getModHD() ? m_iHiddenTimeDiff : 0);
	m_iDelta = m_iTime - curPos;

	if (curPos >= m_iTime - m_iObjectTime && curPos < m_iTime+m_iObjectDuration ) // 1 ms fudge by using >=, shouldn't really be a problem
	{
		// this calculates the default alpha and approach circle scale for playing without mods
		float scale = (float)m_iDelta / (float)m_iApproachTime;
		m_fApproachScale = 1 + scale * osu_approach_scale_multiplier.getFloat();

		m_fFadeInScale = ((float)m_iDelta - (float)m_iApproachTime + (float)m_iFadeInTime) / (float)m_iFadeInTime;
		m_fAlpha = clamp<float>(1.0f - m_fFadeInScale, 0.0f, 1.0f);

		m_bVisible = true;
	}
	else
	{
		m_fApproachScale = 1.0f;
		m_bVisible = false;
	}

	if (m_hitResults.size() > 0 && engine->getTime() > m_hitResults[0].anim)
		m_hitResults.erase(m_hitResults.begin());
}

void OsuHitObject::addHitResult(OsuScore::HIT result, long delta, Vector2 posRaw, float targetDelta, float targetAngle, bool ignoreOnHitErrorBar, bool ignoreCombo)
{
	if (m_beatmap->getOsu()->getModTarget() && result != OsuScore::HIT::HIT_MISS && targetDelta >= 0.0f)
	{
		if (targetDelta < osu_mod_target_300_percent.getFloat() && (result == OsuScore::HIT::HIT_300 || result == OsuScore::HIT::HIT_100))
			result = OsuScore::HIT::HIT_300;
		else if (targetDelta < osu_mod_target_100_percent.getFloat())
			result = OsuScore::HIT::HIT_100;
		else if (targetDelta < osu_mod_target_50_percent.getFloat())
			result = OsuScore::HIT::HIT_50;
		else
			result = OsuScore::HIT::HIT_MISS;

		m_beatmap->getOsu()->getHUD()->addTarget(targetDelta, targetAngle);
	}

	m_beatmap->addHitResult(result, delta, ignoreOnHitErrorBar, false, ignoreCombo);

	HITRESULTANIM hitresult;
	hitresult.result = result;
	hitresult.rawPos = posRaw;
	hitresult.delta = delta;
	hitresult.defaultduration = osu_hitresult_duration.getFloat();
	hitresult.defaultanim = engine->getTime() + hitresult.defaultduration;
	hitresult.duration = hitresult.defaultduration;

	// special case: animated hitresults don't fade out, and their animation duration is exactly equal to how many frames they have, multiplied by their animation framerate
	const float maxAnimDuration = 4.0f;
	const float speedCompensation = 1.0f / clamp<float>(m_beatmap->getSpeedMultiplier(), 0.1f, 999.0f);
	switch (result)
	{
	case OsuScore::HIT::HIT_300:
		if (m_beatmap->getSkin()->getHit300()->getNumImages() > 1)
			hitresult.duration = clamp<float>(m_beatmap->getSkin()->getHit300()->getNumImages() * m_beatmap->getSkin()->getHit300()->getFrameDuration() * speedCompensation, 0.0f, maxAnimDuration);
		break;
	case OsuScore::HIT::HIT_100:
		if (m_beatmap->getSkin()->getHit100()->getNumImages() > 1)
			hitresult.duration = clamp<float>(m_beatmap->getSkin()->getHit100()->getNumImages() * m_beatmap->getSkin()->getHit100()->getFrameDuration() * speedCompensation, 0.0f, maxAnimDuration);
		break;
	case OsuScore::HIT::HIT_50:
		if (m_beatmap->getSkin()->getHit50()->getNumImages() > 1)
			hitresult.duration = clamp<float>(m_beatmap->getSkin()->getHit50()->getNumImages() * m_beatmap->getSkin()->getHit300()->getFrameDuration() * speedCompensation, 0.0f, maxAnimDuration);
		break;
	case OsuScore::HIT::HIT_MISS:
		if (m_beatmap->getSkin()->getHit0()->getNumImages() > 1)
			hitresult.duration = clamp<float>(m_beatmap->getSkin()->getHit0()->getNumImages() * m_beatmap->getSkin()->getHit0()->getFrameDuration() * speedCompensation, 0.0f, maxAnimDuration);
		break;
	}

	hitresult.anim = engine->getTime() + hitresult.duration;

	m_hitResults.push_back(hitresult);
}

void OsuHitObject::onReset(long curPos)
{
	m_bMisAim = false;
	m_iAutopilotDelta = 0;

	m_hitResults.clear();
}
