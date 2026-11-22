/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_top_bar.h"

#include "settings/sections/settings_extera.h"

#include "extera/extera_badges.h"
#include "extera/extera_config.h"
#include "extera/extera_plugin_trust.h"

#include "api/api_peer_colors.h"
#include "api/api_peer_photo.h"
#include "api/api_user_privacy.h"
#include "apiwrap.h"
#include "base/call_delayed.h"
#include "base/event_filter.h"
#include "base/timer_rpl.h"
#include "base/timer.h"
#include "base/unixtime.h"
#include "boxes/peers/edit_peer_info_box.h" // EditPeerInfoBox::Available.
#include "boxes/peers/edit_forum_topic_box.h"
#include "boxes/peers/manage_community_box.h"
#include "boxes/moderate_messages_box.h"
#include "boxes/report_messages_box.h"
#include "boxes/star_gift_box.h"
#include "calls/calls_instance.h"
#include "chat_helpers/stickers_lottie.h"
#include "core/application.h"
#include "core/shortcuts.h"
#include "data/components/recent_shared_media_gifts.h"
#include "data/data_birthday.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_document_media.h"
#include "data/data_document.h"
#include "data/data_emoji_statuses.h"
#include "data/data_forum_topic.h"
#include "data/data_forum.h"
#include "data/data_peer_values.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_saved_sublist.h"
#include "data/data_session.h"
#include "data/data_star_gift.h"
#include "data/data_stories.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "editor/photo_editor_common.h"
#include "editor/photo_editor_layer_widget.h"
#include "editor/video/video_editor_layer.h"
#include "history/history.h"
#include "info/info_memento.h"
#include "info/profile/info_profile_badge.h"
#include "info/profile/info_profile_birthday_effect.h"
#include "info/profile/info_profile_cover.h" // LargeCustomEmojiMargins
#include "info/profile/info_profile_pills.h"
#include "info/profile/info_profile_status_label.h"
#include "info/profile/info_profile_top_bar_action_button.h"
#include "info/profile/info_profile_values.h"
#include "info/userpic/info_userpic_emoji_builder_common.h"
#include "info/userpic/info_userpic_emoji_builder_common.h"
#include "info/userpic/info_userpic_emoji_builder_menu_item.h"
#include "lang/lang_keys.h"
#include "lottie/lottie_animation.h"
#include "lottie/lottie_multi_player.h"
#include "main/main_session.h"
#include "media/audio/media_audio.h"
#include "media/player/media_player_instance.h"
#include "menu/menu_mute.h"
#include "settings/settings_credits_graphics.h"
#include "settings/sections/settings_information.h"
#include "settings/sections/settings_premium.h"
#include "ui/boxes/show_or_premium_box.h"
#include "ui/color_contrast.h"
#include "ui/controls/stars_rating.h"
#include "ui/controls/swipe_handler.h"
#include "ui/controls/userpic_button.h"
#include "ui/effects/animated_string.h"
#include "ui/effects/animations.h"
#include "ui/effects/upload_progress_overlay.h"
#include "ui/effects/outline_segments.h"
#include "ui/effects/round_checkbox.h"
#include "ui/empty_userpic.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/peer/video_userpic_player.h"
#include "ui/rect.h"
#include "ui/effects/numbers_animation.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/top_background_gradient.h"
#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/glare_tooltip.h"
#include "ui/widgets/horizontal_fit_container.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/marquee_label.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/shadow.h"
#include "ui/widgets/tooltip.h"
#include "ui/wrap/fade_wrap.h"
#include "window/themes/window_theme.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"
#include "ui/toast/toast.h"
#include "boxes/sticker_set_box.h"
#include "styles/style_chat.h"
#include "styles/style_info.h"
#include "styles/style_info_profile_top_bar.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

#include <QtGui/QClipboard>
#include <QtGui/QConicalGradient>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>

#include <array>
#include <cmath>

namespace Info::Profile {
namespace {

class Userpic final
	: public Ui::AbstractButton
	, public Ui::AbstractTooltipShower {
public:
	Userpic(QWidget *parent, Fn<bool()> hasStories)
	: Ui::AbstractButton(parent)
	, _hasStories(std::move(hasStories)) {
		installEventFilter(this);
		setAccessibleName(tr::lng_mediaview_profile_photo(tr::now));
	}

	QString tooltipText() const override {
		return _hasStories() ? tr::lng_view_button_story(tr::now) : QString();
	}

	QPoint tooltipPos() const override {
		return QCursor::pos();
	}

	bool tooltipWindowActive() const override {
		return Ui::AppInFocus() && Ui::InFocusChain(window());
	}

protected:
	bool eventFilter(QObject *obj, QEvent *e) override {
		if (obj == this && e->type() == QEvent::Enter && _hasStories()) {
			Ui::Tooltip::Show(1000, this);
		}
		return Ui::AbstractButton::eventFilter(obj, e);
	}

private:
	Fn<bool()> _hasStories;

};

constexpr auto kWaitBeforeGiftBadge = crl::time(1000);
constexpr auto kGiftBadgeGlares = 3;
constexpr auto kMinPatternRadius = 8;
constexpr auto kStoryOutlineFadeEnd = 0.4;
constexpr auto kStoryOutlineFadeRange = 1. - kStoryOutlineFadeEnd;
constexpr auto kSwapMoveAmplitude = 0.3;

using AnimatedPatternPoint = TopBar::AnimatedPatternPoint;

[[nodiscard]] QColor ContentColorOverBackground(const QColor &background) {
	return Ui::IsLightBackground(background)
		? QColor(Qt::black)
		: st::groupCallMembersFg->c;
}

class BackdropIconButton final : public Ui::IconButton {
public:
	using Ui::IconButton::IconButton;

	void setBackdropColor(std::optional<QColor> color);

protected:
	void paintEvent(QPaintEvent *e) override;
	void onStateChanged(State was, StateChangeSource source) override;

private:
	std::optional<QColor> _backdropColor;
	style::owned_color _rippleColor = style::owned_color(QColor());
	Ui::Animations::Simple _overAnimation;

};

void BackdropIconButton::setBackdropColor(std::optional<QColor> color) {
	_backdropColor = color;
	setIconColorOverride(color);
	if (color) {
		_rippleColor.update(anim::with_alpha(
			*color,
			st::infoProfileTopBarBackdropRippleOpacity));
	}
	setRippleColorOverride(color ? &_rippleColor.color() : nullptr);
	update();
}

void BackdropIconButton::paintEvent(QPaintEvent *e) {
	const auto shown = _backdropColor
		? _overAnimation.value(isOver() ? 1. : 0.)
		: 0.;
	if (shown > 0.) {
		auto p = QPainter(this);
		auto hq = PainterHighQualityEnabler(p);
		p.setOpacity(shown);
		p.setPen(Qt::NoPen);
		p.setBrush(_rippleColor.color());
		p.drawEllipse(QRect(
			st().rippleAreaPosition,
			QSize(st().rippleAreaSize, st().rippleAreaSize)));
	}
	Ui::IconButton::paintEvent(e);
}

void BackdropIconButton::onStateChanged(
		State was,
		StateChangeSource source) {
	Ui::IconButton::onStateChanged(was, source);

	const auto over = isOver();
	if (over != ((was & StateFlag::Over) != 0)) {
		_overAnimation.start(
			[=] { update(); },
			over ? 0. : 1.,
			over ? 1. : 0.,
			st::universalDuration);
	}
}

[[nodiscard]] not_null<BackdropIconButton*> Backdrop(
		not_null<Ui::IconButton*> button) {
	return static_cast<BackdropIconButton*>(button.get());
}

struct PatternColors {
	QColor patternColor;
	bool useOverlayBlend = false;
};

[[nodiscard]] PatternColors CalculatePatternColors(
		const std::optional<Data::ColorProfileSet> &colorProfile,
		const std::shared_ptr<Data::EmojiStatusCollectible> &collectible,
		const std::optional<QColor> &edgeColor,
		bool isDark) {
	if (collectible && collectible->patternColor.isValid()) {
		auto blended = Ui::BlendColors(
			collectible->patternColor,
			Qt::black,
			isDark ? (140. / 255) : (160. / 255));
		auto result = !edgeColor
			? std::move(blended)
			: (Ui::CountContrast(blended, *edgeColor)
				> Ui::CountContrast(collectible->patternColor, *edgeColor))
			? std::move(blended)
			: collectible->patternColor;
		return {
			.patternColor = std::move(result),
			// .patternColor = collectible->patternColor.lighter(isDark
			// 	? 140
			// 	: 160),
			.useOverlayBlend = false
		};
	}
	if (colorProfile && !colorProfile->bg.empty()) {
		return {
			.patternColor = QColor(0, 0, 0, int(0.6 * 255)),
			.useOverlayBlend = true
		};
	}
	const auto baseWhite = isDark ? 0.5 : 0.3;
	return {
		.patternColor = QColor::fromRgbF(
			baseWhite,
			baseWhite,
			baseWhite,
			0.6),
		.useOverlayBlend = false
	};
}

[[nodiscard]] std::vector<AnimatedPatternPoint> GenerateAnimatedPattern(
		const QRect &userpicRect) {
	auto points = std::vector<TopBar::AnimatedPatternPoint>();
	points.reserve(18); // 6 + 6 + 4 + 2.
	const auto ax = float64(userpicRect.x());
	const auto ay = float64(userpicRect.y());
	const auto aw = float64(userpicRect.width());
	const auto ah = float64(userpicRect.height());
	const auto acx = ax + aw / 2.;
	const auto acy = ay + ah / 2.;

	constexpr auto kPaddingScale = 0.8;

	const auto padding24 = style::ConvertFloatScale(24. * kPaddingScale);
	const auto padding16 = style::ConvertFloatScale(16. * kPaddingScale);
	const auto padding8 = style::ConvertFloatScale(8. * kPaddingScale);
	const auto padding12 = style::ConvertFloatScale(12. * kPaddingScale);
	const auto padding48 = style::ConvertFloatScale(48. * kPaddingScale);
	const auto padding96 = style::ConvertFloatScale(96. * kPaddingScale);
	static const auto kCos120 = std::cos(M_PI * 120. / 180.);
	static const auto kCos160 = std::cos(M_PI * 160. / 180.);
	const auto r48Cos120 = (padding48 + aw / 2.) * kCos120;
	const auto r16Cos160 = (padding16 + ah / 2.) * kCos160;

	// First ring.
	points.push_back({ { acx, ay - padding24 }, 20, 0.02, 0.42 });
	points.push_back({ { acx, ay + ah + padding24 }, 20, 0.00, 0.32 });
	points.push_back({ { ax - padding16, acy - ah / 4 - padding8 }, 23, 0.00, 0.40 });
	points.push_back({ { ax + aw + padding16, acy - ah / 4 - padding8 }, 18, 0.00, 0.40 });
	points.push_back({ { ax - padding16, acy + ah / 4 + padding8 }, 24, 0.00, 0.40 });
	points.push_back({ { ax + aw + padding16 - 4, acy + ah / 4 + padding8 }, 24, 0.00, 0.40 });

	// Second ring.
	points.push_back({ { ax - padding48, acy }, 19, 0.14, 0.60 });
	points.push_back({ { ax + aw + padding48, acy }, 19, 0.16, 0.64 });
	points.push_back({ { acx + r48Cos120, ay - padding48 + padding12 }, 17, 0.14, 0.70 });
	points.push_back({ { acx - r48Cos120, ay - padding48 + padding12 }, 17, 0.14, 0.90 });
	points.push_back({ { acx + r48Cos120, ay + ah + padding48 - padding12 }, 20, 0.20, 0.75 });
	points.push_back({ { acx - r48Cos120, ay + ah + padding48 - padding12 }, 20, 0.20, 0.85 });

	// Third ring.
	points.push_back({ { ax - padding48 - padding8, acy + r16Cos160 }, 20, 0.09, 0.45 });
	points.push_back({ { ax + aw + padding48 + padding8, acy + r16Cos160 }, 19, 0.09, 0.45 });
	points.push_back({ { ax - padding48 - padding8, acy - r16Cos160 }, 21, 0.09, 0.45 });
	points.push_back({ { ax + aw + padding48 + padding8, acy - r16Cos160 }, 18, 0.11, 0.45 });

	// Fourth ring.
	points.push_back({ { ax - padding96, acy }, 19, 0.14, 0.75 });
	points.push_back({ { ax + aw + padding96, acy }, 19, 0.20, 0.80 });

	return points;
}

Ui::RpWidget *SetupTabBarBackground(
		not_null<Ui::FadeWrap<Ui::RpWidget>*> bar,
		const style::color &bg,
		const bool &roundEdges) {
	bar->setDuration(st::infoTopBarDuration);
	bar->toggle(false, anim::type::instant);

	const auto inner = bar->entity();
	inner->paintRequest(
	) | rpl::on_next([=, &bg, &roundEdges] {
		auto p = QPainter(inner);
		auto hq = PainterHighQualityEnabler(p);
		const auto radius = roundEdges ? st::boxRadius : 0;
		p.setPen(Qt::NoPen);
		p.setBrush(bg);
		p.drawRoundedRect(
			inner->rect() + QMargins(0, 0, 0, radius),
			radius,
			radius);
		const auto line = st::lineWidth;
		p.fillRect(
			0,
			inner->height() - line,
			inner->width(),
			line,
			st::shadowFg);
	}, inner->lifetime());
	return inner;
}

struct AdaptiveProfileColors {
	QColor center;
	QColor edge;
};

[[nodiscard]] AdaptiveProfileColors ExtractAvatarProfileColors(
		const QImage &image,
		bool nightMode) {
	if (image.isNull()) {
		return {};
	}
	const auto small = image.scaled(
		24,
		24,
		Qt::IgnoreAspectRatio,
		Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
	constexpr auto kBuckets = 12;
	auto bucketWeight = std::array<float64, kBuckets>{};
	auto bucketR = std::array<float64, kBuckets>{};
	auto bucketG = std::array<float64, kBuckets>{};
	auto bucketB = std::array<float64, kBuckets>{};
	auto totalWeight = 0.;
	auto totalR = 0.;
	auto totalG = 0.;
	auto totalB = 0.;

	const auto w = small.width();
	const auto h = small.height();
	const auto cx = (w - 1) * 0.5;
	const auto cy = (h - 1) * 0.5;
	const auto invR = 1. / std::max(cx, 1.);

	for (auto y = 0; y != h; ++y) {
		const auto dy = (y - cy) * invR;
		for (auto x = 0; x != w; ++x) {
			const auto dx = (x - cx) * invR;
			if (dx * dx + dy * dy > 1.02) {
				continue;
			}
			const auto px = small.pixel(x, y);
			const auto alpha = qAlpha(px);
			if (alpha < 96) {
				continue;
			}
			const auto color = QColor::fromRgb(px);
			auto hue = 0.f;
			auto sat = 0.f;
			auto val = 0.f;
			color.getHsvF(&hue, &sat, &val);
			const auto r = color.redF();
			const auto g = color.greenF();
			const auto b = color.blueF();
			const auto alphaW = alpha / 255.;
			const auto lumaPref = 1. - std::abs(float64(val) - 0.55) * 0.72;
			const auto weight = alphaW
				* (0.14 + float64(sat) * float64(sat) * 2.4)
				* std::max(lumaPref, 0.15);

			totalWeight += alphaW;
			totalR += r * alphaW;
			totalG += g * alphaW;
			totalB += b * alphaW;

			if (hue >= 0.f && sat >= 0.12f && val >= 0.08f) {
				const auto idx = std::clamp(
					int(hue * kBuckets) % kBuckets,
					0,
					kBuckets - 1);
				bucketWeight[idx] += weight;
				bucketR[idx] += r * weight;
				bucketG[idx] += g * weight;
				bucketB[idx] += b * weight;
			}
		}
	}

	auto bestBucket = -1;
	auto bestScore = 0.;
	for (auto i = 0; i != kBuckets; ++i) {
		if (bucketWeight[i] > bestScore) {
			bestScore = bucketWeight[i];
			bestBucket = i;
		}
	}

	auto base = QColor();
	if (bestBucket >= 0 && bestScore > 0.25) {
		base = QColor::fromRgbF(
			float(bucketR[bestBucket] / bestScore),
			float(bucketG[bestBucket] / bestScore),
			float(bucketB[bestBucket] / bestScore));
	} else if (totalWeight > 0.) {
		base = QColor::fromRgbF(
			float(totalR / totalWeight),
			float(totalG / totalWeight),
			float(totalB / totalWeight));
	} else {
		base = QColor(64, 118, 198);
	}

	auto bh = 0.f;
	auto bs = 0.f;
	auto bl = 0.f;
	base.getHslF(&bh, &bs, &bl);
	if (bh < 0.f || bs < 0.08f) {
		return nightMode
			? AdaptiveProfileColors{
				.center = QColor::fromHslF(0.58f, 0.22f, 0.34f),
				.edge = QColor::fromHslF(0.58f, 0.18f, 0.17f),
			}
			: AdaptiveProfileColors{
				.center = QColor::fromHslF(0.58f, 0.34f, 0.52f),
				.edge = QColor::fromHslF(0.58f, 0.36f, 0.39f),
			};
	}

	if (nightMode) {
		return {
			.center = QColor::fromHslF(
				bh,
				float(std::clamp(float64(bs) * 1.08, 0.38, 0.78)),
				float(std::clamp(float64(bl), 0.32, 0.44))),
			.edge = QColor::fromHslF(
				bh,
				float(std::clamp(float64(bs) * 0.88, 0.30, 0.64)),
				float(std::clamp(float64(bl) * 0.50, 0.15, 0.22))),
		};
	}
	return {
		.center = QColor::fromHslF(
			bh,
			float(std::clamp(float64(bs) * 1.05, 0.44, 0.82)),
			float(std::clamp(float64(bl), 0.48, 0.58))),
		.edge = QColor::fromHslF(
			bh,
			float(std::clamp(float64(bs) * 0.94, 0.40, 0.74)),
			float(std::clamp(float64(bl) * 0.80, 0.34, 0.43))),
	};
}

[[nodiscard]] QColor MakeVividRimColor(const QColor &base) {
	if (!base.isValid()) {
		return QColor(140, 200, 255);
	}
	auto h = 0.f;
	auto s = 0.f;
	auto l = 0.f;
	base.getHslF(&h, &s, &l);
	if (h < 0.f) {
		return QColor(235, 242, 255);
	}
	return QColor::fromHslF(
		h,
		float(std::clamp(float64(s) * 1.25, 0.72, 0.96)),
		float(std::clamp(float64(l) * 1.15, 0.60, 0.74)));
}

[[nodiscard]] float64 GiftBubbleRadius() {
	return float64(style::ConvertFloatScale(17.0));
}

} // namespace

TopBar::TopBar(
	not_null<Ui::RpWidget*> parent,
	Descriptor descriptor)
: RpWidget(parent)
, _peer(descriptor.peer
	? descriptor.peer
	: descriptor.key.peer())
, _topic(descriptor.key.topic())
, _key(descriptor.key)
, _wrap(std::move(descriptor.wrap))
, _st(st::infoTopBar)
, _source(descriptor.source)
, _savedMessages(_key.savedMessages() != nullptr)
, _badgeTooltipHide(
	std::make_unique<base::Timer>([=] { hideBadgeTooltip(); }))
, _botVerify(std::make_unique<Badge>(
	this,
	st::infoBotVerifyBadge,
	&_peer->session(),
	BotVerifyBadgeForPeer(_peer),
	nullptr,
	Fn<bool()>([=, controller = descriptor.controller] {
		return controller->isGifPausedAtLeastFor(
			Window::GifPauseReason::Layer);
	})))
, _badgeContent(_savedMessages
	? rpl::producer<Badge::Content>(rpl::single(Badge::Content()))
	: BadgeContentForPeer(_peer))
, _gifPausedChecker([=, controller = descriptor.controller] {
	return controller->isGifPausedAtLeastFor(Window::GifPauseReason::Layer);
})
, _badge(std::make_unique<Badge>(
	this,
	st::infoPeerBadge,
	&_peer->session(),
	_badgeContent.value(),
	nullptr,
	_gifPausedChecker))
, _verified(std::make_unique<Badge>(
	this,
	st::infoPeerBadge,
	&_peer->session(),
	VerifiedContentForPeer(_peer),
	nullptr,
	_gifPausedChecker))
, _hasActions(!_savedMessages
	&& descriptor.source != Source::Stories
	&& descriptor.source != Source::Preview
	&& (_wrap.current() != Wrap::Side || !_peer->isNotificationsUser()))
, _minForProgress([&] {
	QWidget::setMinimumHeight(st::infoLayerTopBarHeight);
	QWidget::setMaximumHeight(_savedMessages
		? st::infoLayerTopBarHeight
		: _hasActions
		? st::infoProfileTopBarHeightMax
		: st::infoProfileTopBarNoActionsHeightMax);
	return QWidget::minimumHeight()
		+ (!_hasActions
			? 0
			: st::infoProfileTopBarActionButtonsHeight);
}())
, _title(this, nameValue(), _st.title)
, _starsRating((_peer->isUser() && !_savedMessages)
	? std::make_unique<Ui::StarsRating>(
		this,
		descriptor.controller->uiShow(),
		_peer->isSelf() ? QString() : _peer->shortName(),
		Data::StarsRatingValue(_peer),
		(_peer->isSelf()
			? [=] { return _peer->owner().pendingStarsRating(); }
			: Fn<Data::StarsRatingPending()>()))
	: nullptr)
, _status(this, QString(), statusStyle())
, _statusLabel(std::make_unique<StatusLabel>(_status.data(), _peer))
, _customStatus(std::move(descriptor.customStatus))
, _showLastSeen(
	this,
	object_ptr<Ui::RoundButton>(
		this,
		tr::lng_status_lastseen_when(),
		st::infoProfileTopBarShowLastSeen))
, _forumButton([&, controller = descriptor.controller] {
	const auto topic = _key.topic();
	if (!topic) {
		return object_ptr<Ui::RoundButton>{ nullptr };
	}
	auto owned = object_ptr<Ui::RoundButton>(
		this,
		rpl::single(QString()),
		st::infoProfileTopBarTopicStatusButton);
	owned->setText(Info::Profile::NameValue(
		_peer
	) | rpl::map([=](const QString &name) {
		return TextWithEntities(name)
			.append(' ')
			.append(Ui::Text::IconEmoji(&st::textMoreIconEmoji, QString()));
	}));
	owned->setClickedCallback([=, peer = _peer] {
		if (const auto forum = peer->forum()) {
			if (peer->useSubsectionTabs()) {
				controller->searchInChat(forum->history());
			} else if (controller->adaptive().isOneColumn()) {
				controller->showForum(forum);
			} else {
				controller->showPeerHistory(peer->id);
			}
		} else {
			controller->showPeerHistory(peer->id);
		}
	});
	return owned;
}())
, _backToggles(std::move(descriptor.backToggles)) {
	setObjectName(u"profileTopBar"_q);
	_peer->updateFull();
	_communityEffect = (_source == Source::Community);
	if (const auto broadcast = _peer->monoforumBroadcast()) {
		broadcast->updateFull();
	}
	const auto controller = descriptor.controller;

	if (_peer->isMegagroup() || _peer->isChat()) {
		_statusLabel->setMembersLinkCallback([=, peer = _peer] {
			const auto topic = _key.topic();
			const auto sublist = _key.sublist();
			const auto shown = sublist
				? sublist->sublistPeer().get()
				: peer.get();
			const auto section = Section::Type::Members;
			controller->showSection(topic
				? std::make_shared<Info::Memento>(topic, section)
				: std::make_shared<Info::Memento>(shown, section));
		});
	}
	_statusLabel->setHiddenLinkCallback([=] {
		controller->showToast(Ui::Toast::Config{
			.title = tr::lng_community_hidden_chat_title(tr::now),
			.text = tr::lng_community_hidden_chat_about(
				tr::now,
				tr::marked),
			.icon = &st::infoStatusHiddenToastIcon,
		});
	});
	if (!_peer->isMegagroup() && !_topic) {
		setupStatusWithRating();
	}

	if (!_topic) {
		setupShowLastSeen(controller);
	}

	bindStatus();
	setupPills(controller, std::move(descriptor.showSavedMusic));

	_title->setContextCopyText(tr::lng_profile_copy_fullname(tr::now));
	_title->setSelectable(true);

	auto badgeUpdates = rpl::producer<rpl::empty_value>();
	if (_badge) {
		badgeUpdates = rpl::merge(
			std::move(badgeUpdates),
			_badge->updated());

		_badge->setPremiumClickCallback([controller, peer = _peer] {
			::Settings::ShowEmojiStatusPremium(controller, peer);
		});
	}
	if (_verified) {
		_verified->setPremiumClickCallback([controller, peer = _peer] {
			using Kind = Extera::BadgeKind;
			const auto custom = Extera::BadgeCustomText(peer);
			const auto name = peer->name();
			const auto kind = Extera::BadgeKindFor(peer);
			const auto exdToast = [=](const QString &text) {
				const auto supporter = (kind == Kind::Donor)
					|| (kind == Kind::Supporter)
					|| (kind == Kind::Custom);
				::Settings::ShowExteraBadgeBox(
					controller,
					peer,
					text,
					supporter);
			};
			if (Extera::PluginTrust::IsTrustedChannel(peer)) {
				exdToast(tr::lng_exd_badge_trusted(tr::now, lt_name, name));
				return;
			}
			if (!custom.isEmpty()
				&& kind != Kind::Developer
				&& kind != Kind::OfficialChannel
				&& kind != Kind::Supporter) {
				exdToast(custom);
				return;
			}
			switch (kind) {
			case Kind::Porter:
				exdToast(tr::lng_extera_badge_developer(
					tr::now, lt_name, name));
				break;
			case Kind::PorterChannel:
				exdToast(tr::lng_extera_badge_resource(
					tr::now, lt_name, name));
				break;
			case Kind::Developer:
				exdToast(tr::lng_extera_badge_extera_developer(
					tr::now, lt_name, name));
				break;
			case Kind::OfficialChannel:
				exdToast(tr::lng_extera_badge_extera_resource(
					tr::now, lt_name, name));
				break;
			case Kind::Supporter:
				exdToast(tr::lng_extera_badge_supporter(
					tr::now, lt_name, name));
				break;
			case Kind::Custom:
				exdToast(
					tr::lng_extera_badge_supporter(tr::now, lt_name, name));
				break;
			case Kind::Bughunter:
				exdToast(tr::lng_exd_badge_bughunter(
					tr::now, lt_name, name));
				break;
			case Kind::Helper:
				exdToast(tr::lng_exd_badge_helper(
					tr::now, lt_name, name));
				break;
			case Kind::Donor:
				exdToast(tr::lng_exd_badge_donor(
					tr::now, lt_name, name));
				break;
			case Kind::None:
				break;
			}
		});
		badgeUpdates = rpl::merge(
			std::move(badgeUpdates),
			_verified->updated());
	}
	if (_botVerify) {
		badgeUpdates = rpl::merge(
			std::move(badgeUpdates),
			_botVerify->updated());
	}
	_title->naturalWidthValue() | rpl::on_next([=](int w) {
		_title->resizeToWidth(w);
	}, _title->lifetime());
	badgeUpdates = rpl::merge(
		std::move(badgeUpdates),
		nameValue() | rpl::to_empty,
		_backToggles.value() | rpl::to_empty);
	std::move(badgeUpdates) | rpl::on_next([=] {
		updateLabelsPosition();
	}, _title->lifetime());

	setupUniqueBadgeTooltip();
	setupButtons(controller, descriptor.source);
	setupSwipeBack(controller);
	setupUserpicButton(controller);
	if (_hasActions) {
		_peer->session().changes().peerFlagsValue(
			_peer,
			Data::PeerUpdate::Flag::FullInfo
				| Data::PeerUpdate::Flag::ChannelAmIn
		) | rpl::on_next([=] {
			setupActions(controller);
		}, lifetime());
	}
	if (_source == Source::Community) {
		Shortcuts::Requests(
		) | rpl::filter([=] {
			return Core::App().activeWindow() == &controller->window();
		}) | rpl::on_next([=](not_null<Shortcuts::Request*> request) {
			using Command = Shortcuts::Command;
			request->check(Command::Search, 1) && request->handle([=] {
				searchInCommunity(controller);
				return true;
			});
		}, lifetime());
	}
	setupStoryOutline();
	if (_topic) {
		_topicIconView = std::make_unique<TopicIconView>(
			_topic,
			_gifPausedChecker,
			[=] { update(); });
	} else if (!_savedMessages) {
		updateVideoUserpic();
	}
	rpl::merge(
		windowActiveValue() | rpl::to_empty,
		descriptor.controller->gifPauseLevelChanged()
	) | rpl::on_next([=] {
		update();
	}, lifetime());

	rpl::merge(
		style::PaletteChanged(),
		_peer->session().data().giftUpdates() | rpl::filter([=](
				const Data::GiftUpdate &update) {
			if (update.action == Data::GiftUpdate::Action::Pin) {
				if (_peer->isSelf() && update.id.isUser()) {
					return true;
				}
				if (_peer == update.id.chat()) {
					return true;
				}
			}
			if (update.action == Data::GiftUpdate::Action::Unpin) {
				for (const auto &gift : _pinnedToTopGifts) {
					if (gift.manageId == update.id) {
						return true;
					}
				}
			}
			return false;
		}) | rpl::to_empty,
		_peer->session().changes().peerFlagsValue(
			_peer,
			Data::PeerUpdate::Flag::EmojiStatus
				| Data::PeerUpdate::Flag::ColorProfile) | rpl::to_empty
	) | rpl::on_next([=] {
		if (_pinnedToTopGiftsFirstTimeShowed) {
			_peer->session().recentSharedGifts().clearLastRequestTime(_peer);
			setupPinnedToTopGifts(controller);
		} else {
			updateCollectibleStatus();
		}
	}, lifetime());

	std::move(
		descriptor.showFinished
	) | rpl::take(1) | rpl::on_next([=] {
		setupPinnedToTopGifts(controller);
		setupBirthdayEffect();
	}, lifetime());

	if (_forumButton) {
		_forumButton->show();
	}

	_v2AnimTimer = std::make_unique<base::Timer>([=] {
		stepProfileV2Physics();
	});
	_v2MusicPlaying = Media::Player::ShowPauseIcon(
		Media::Player::instance()->getState(AudioMsgId::Type::Song).state);
	Media::Player::instance()->updatedNotifier(
	) | rpl::on_next([=](const Media::Player::TrackState &state) {
		if (state.id.type() != AudioMsgId::Type::Song) {
			return;
		}
		const auto playing = Media::Player::ShowPauseIcon(state.state);
		if (_v2MusicPlaying != playing) {
			_v2MusicPlaying = playing;
			updateProfileV2AnimState();
		}
	}, lifetime());
	Extera::Config::Instance().changes(
	) | rpl::filter([](const QString &key) {
		return key.startsWith(u"profile_v2_"_q);
	}) | rpl::on_next([=] {
		refreshProfileV2State();
	}, lifetime());
	style::PaletteChanged(
	) | rpl::on_next([=] {
		_adaptiveAvatarKey = InMemoryKey();
		_adaptiveCenterColor = QColor();
		_adaptiveEdgeColor = QColor();
	}, lifetime());
	updateProfileV2AnimState();
}

void TopBar::setupBirthdayEffect() {
	const auto user = _peer->asUser();
	if (!user || !Data::IsBirthdayToday(user->birthday())) {
		return;
	} else if (_wrap.current() == Wrap::Side) {
		return;
	}
	const auto container = static_cast<Ui::RpWidget*>(parentWidget());
	if (!container) {
		return;
	}
	StartProfileBirthdayEffect(
		container,
		user,
		[weak = base::make_weak(this)] {
			const auto strong = weak.get();
			if (!strong) {
				return QRect();
			}
			const auto geometry = strong->userpicGeometry();
			return QRect(
				strong->mapToParent(geometry.topLeft()),
				geometry.size());
		},
		_gifPausedChecker);
}

void TopBar::adjustColors(const std::optional<QColor> &edgeColor) {
	constexpr auto kMinContrast = 5.5;
	const auto shouldOverride = [&](const style::color &color) {
		return edgeColor
			&& (kMinContrast > Ui::CountContrast(color->c, *edgeColor));
	};
	const auto collectible = effectiveCollectible();
	const auto shouldOverrideTitle = shouldOverride(_title->st().textFg);
	const auto shouldOverrideStatus = shouldOverrideTitle
		|| shouldOverride(_status->st().textFg);
	_title->setTextColorOverride(collectible
		? collectible->textColor
		: shouldOverrideTitle
		? std::optional<QColor>(st::groupCallMembersFg->c)
		: std::nullopt);
	_tabSubtitleOverride = collectible
		? collectible->textColor
		: shouldOverrideStatus
		? std::optional<QColor>(st::groupCallVideoSubTextFg->c)
		: std::nullopt;
	update();
	if (_showLastSeen->toggled()) {
		if (shouldOverrideTitle) {
			const auto st = mapActionStyle(edgeColor);
			_showLastSeen->entity()->setBrushOverride(st.bgColor);
			_showLastSeen->entity()->setTextFgOverride(st.fgColor);
		} else {
			_showLastSeen->entity()->setBrushOverride(std::nullopt);
			_showLastSeen->entity()->setTextFgOverride(std::nullopt);
		}
	}
	{
		const auto membersLinkCallback = _statusLabel->membersLinkCallback();
		const auto hiddenLinkCallback = _statusLabel->hiddenLinkCallback();
		const auto onlineCount = _statusLabel->onlineCount();
		{
			_statusLabel = nullptr;
			delete _status.release();
		}
		if (shouldOverrideStatus) {
			const auto copySt = [&](const style::FlatLabel &st) {
				auto result = std::make_unique<style::FlatLabel>(
					base::duplicate(st));
				result->palette.linkFg = st::groupCallVideoSubTextFg;
				return result;
			};
			_statusSt = copySt(statusStyle());
			_status.create(this, QString(), *(_statusSt.get()));
		} else {
			_status.create(this, QString(), statusStyle());
		}
		_status->show();
		if (!_peer->isMegagroup() && !_topic) {
			setupStatusWithRating();
		}
		_status->widthValue() | rpl::on_next([=] {
			updateStatusPosition(_progress.current());
		}, _status->lifetime());
		_statusLabel = std::make_unique<StatusLabel>(_status.data(), _peer);
		_statusLabel->setMembersLinkCallback(membersLinkCallback);
		_statusLabel->setHiddenLinkCallback(hiddenLinkCallback);
		if (_customStatus) {
			rpl::duplicate(
				_customStatus
			) | rpl::on_next([=](TextWithEntities text) {
				_status->setMarkedText(std::move(text));
				updateStatusPosition(_progress.current());
			}, _status->lifetime());
		}
		_status->setTextColorOverride(collectible
			? collectible->textColor
			: shouldOverrideStatus
			? std::optional<QColor>(st::groupCallVideoSubTextFg->c)
			: std::nullopt);
		if (!_customStatus) {
			// For the custom status the text is driven by the producer
			// bound above, so let StatusLabel::refresh() (called from
			// setColorized) overwrite _status only when there is no custom
			// status.
			_statusLabel->setColorized(!shouldOverrideStatus);
			_statusLabel->setOnlineCount(onlineCount);
		}
	}

	const auto shouldOverrideBadges = shouldOverride(
		st::infoBotVerifyBadge.premiumFg);
	_botVerify->setOverrideStyle(shouldOverrideBadges
		? _botVerifySt
		? _botVerifySt.get()
		: &st::infoColoredBotVerifyBadge
		: nullptr);
	_badge->setOverrideStyle(shouldOverrideBadges
		? _badgeSt
		? _badgeSt.get()
		: &st::infoColoredPeerBadge
		: nullptr);
	_verified->setOverrideStyle(shouldOverrideBadges
		? _verifiedSt
		? _verifiedSt.get()
		: &st::infoColoredPeerBadge
		: nullptr);

	if (_starsRating) {
		const auto shouldOverrideRating = shouldOverride(st::windowBgActive);
		_starsRating->setCustomColors(
			shouldOverrideRating
				? edgeColor
				: std::nullopt,
			shouldOverrideRating
				? std::make_optional<QColor>(st::windowFgActive->c)
				: std::nullopt);
	}

	_edgeColor = edgeColor;
}

void TopBar::updateCollectibleStatus() {
	const auto collectible = effectiveCollectible();
	const auto colorProfile = effectiveColorProfile();
	_hasGradientBg = (collectible != nullptr)
		|| (colorProfile && colorProfile->bg.size() > 1);
	_solidBg = (colorProfile && colorProfile->bg.size() == 1)
		? std::make_optional(colorProfile->bg.front())
		: std::nullopt;
	_cachedClipPath = QPainterPath();
	_cachedGradient = QImage();
	_basePatternImage = QImage();
	_lastUserpicRect = QRect();
	const auto patternEmojiId = _localPatternEmojiId
		? *_localPatternEmojiId
		: collectible && collectible->patternDocumentId
		? collectible->patternDocumentId
		: _peer->profileBackgroundEmojiId();
	if (patternEmojiId) {
		const auto document = _peer->owner().document(patternEmojiId);
		if (!_patternEmoji
			|| _patternEmoji->entityData()
				!= Data::SerializeCustomEmojiId(document)) {
			_patternEmoji = document->owner().customEmojiManager().create(
				document,
				[=] { update(); },
				Data::CustomEmojiSizeTag::Normal);
		}
	} else {
		_patternEmoji = nullptr;
	}
	if (collectible || _localPatternEmojiId) {
		setupAnimatedPattern();
	} else {
		_animatedPoints.clear();
		_pinnedToTopGifts.clear();
	}
	const auto verifiedFg = [&]() -> std::optional<QColor> {
		if (collectible) {
			return Ui::BlendColors(
				collectible->edgeColor,
				collectible->centerColor,
				0.2);
		}
		if (colorProfile && !colorProfile->palette.empty()) {
			return Ui::BlendColors(
				colorProfile->palette.back(),
				colorProfile->palette.size() == 1 ? Qt::white : Qt::black,
				0.2);
		}
		return std::nullopt;
	}();
	// Updating one badge can relayout neighbors that still use the old styles.
	const auto oldBotVerifySt = std::move(_botVerifySt);
	const auto oldBadgeSt = std::move(_badgeSt);
	const auto oldVerifiedSt = std::move(_verifiedSt);
	if (verifiedFg) {
		const auto copyStVerified = [&](const style::InfoPeerBadge &st) {
			auto result = std::make_unique<style::InfoPeerBadge>(
				base::duplicate(st));
			auto fg = std::make_shared<style::owned_color>(*verifiedFg);
			result->premiumFg = fg->color();
			return std::shared_ptr<style::InfoPeerBadge>(
				result.release(),
				[fg](style::InfoPeerBadge *ptr) { delete ptr; });
			return std::shared_ptr<style::InfoPeerBadge>(result.release());
		};
		const auto copySt = [&](const style::InfoPeerBadge &st) {
			auto result = std::make_unique<style::InfoPeerBadge>(
				base::duplicate(st));
			result->premiumFg = st::groupCallVideoSubTextFg;
			return std::shared_ptr<style::InfoPeerBadge>(result.release());
		};
		_botVerifySt = copySt(st::infoColoredBotVerifyBadge);
		_badgeSt = copySt(st::infoColoredPeerBadge);
		_verifiedSt = copyStVerified(st::infoColoredPeerBadge);
	} else {
		_botVerifySt = nullptr;
		_badgeSt = nullptr;
		_verifiedSt = nullptr;
	}
	const auto useAdaptive = useAdaptiveAvatarBg()
		&& _adaptiveCenterColor.isValid()
		&& _adaptiveEdgeColor.isValid();
	if (useAdaptive) {
		_hasGradientBg = true;
		_solidBg = std::nullopt;
	}
	update();
	adjustColors(collectible
		? std::optional<QColor>(collectible->edgeColor)
		: (colorProfile && !colorProfile->bg.empty())
		? std::optional<QColor>(colorProfile->bg.front())
		: useAdaptive
		? std::optional<QColor>(_adaptiveEdgeColor)
		: std::nullopt);
}

void TopBar::finalizeActions(
		const std::vector<not_null<TopBarActionButton*>> &buttons) {
	_actionsShadow = base::make_unique_q<Ui::RpWidget>(this);
	const auto shadowRaw = _actionsShadow.get();
	shadowRaw->setAttribute(Qt::WA_TransparentForMouseEvents);
	const auto shadow = shadowRaw->lifetime().make_state<Ui::BoxShadow>(
		st::infoProfileTopBarActionButtonShadow);
	const auto shadowShown = shadowRaw->lifetime().make_state<bool>(
		false);
	const auto extend = Ui::BoxShadow::ExtendFor(
		st::infoProfileTopBarActionButtonShadow);
	shadowRaw->paintRequest() | rpl::on_next([=] {
		if (!*shadowShown
			|| (Extera::Bool(u"profile_v2_enabled"_q)
				&& Extera::Bool(u"profile_v2_circular_actions"_q))) {
			return;
		}
		const auto full = st::infoProfileTopBarActionButtonSize;
		const auto progress = std::clamp(
			float64(_actions->height()) / full,
			0.,
			1.);
		if (progress <= 0.) {
			return;
		}
		auto p = QPainter(shadowRaw);
		const auto opacity = shadow->opacity() * progress;
		for (const auto &button : buttons) {
			const auto buttonRect = button->geometry().translated(
				extend.left(),
				extend.top());
			shadow->paint(p, buttonRect, st::boxRadius, opacity);
		}
	}, shadowRaw->lifetime());

	style::PaletteChanged(
	) | rpl::on_next([=] {
		const auto current = _edgeColor.current();
		_edgeColor.force_assign(current);
	}, _actions->lifetime());
	_edgeColor.value() | rpl::map([=](std::optional<QColor> c) {
		return mapActionStyle(c);
	}) | rpl::on_next([=](
			TopBarActionButtonStyle st) {
		for (const auto &button : buttons) {
			button->setStyle(st);
		}
		*shadowShown = st.shadowColor.has_value();
		shadowRaw->update();
	}, _actions->lifetime());
	const auto padding = st::infoProfileTopBarActionButtonsPadding;
	rpl::combine(
		sizeValue(),
		_pillsReserve.value()
	) | rpl::on_next([=](const QSize &size, int reserve) {
		const auto ratio = float64(size.height())
			/ (st::infoProfileTopBarActionButtonsHeight
				+ st::infoLayerTopBarHeight);
		const auto h = st::infoProfileTopBarActionButtonSize;
		const auto resultHeight = (ratio >= 1.)
			? h
			: (ratio <= 0.5)
			? 0
			: int(h * (ratio - 0.5) / 0.5);
		const auto progress = progressForHeight(size.height());
		const auto bottom = size.height()
			- padding.bottom()
			- int(base::SafeRound(reserve * progress));
		_actions->setGeometry(
			padding.left(),
			bottom - resultHeight,
			size.width() - rect::m::sum::h(padding),
			resultHeight);
		if (_pills) {
			_pills->place(bottom + st::infoProfileTopBarPillsSkip, progress);
		}
	}, _actions->lifetime());
	_actions->geometryValue() | rpl::on_next([=](QRect geometry) {
		shadowRaw->setGeometry(geometry.marginsAdded(extend));
		shadowRaw->update();
	}, shadowRaw->lifetime());
	shadowRaw->show();
	_actions->show();
	_actions->raise();
	shadowRaw->stackUnder(_actions.get());
}

void TopBar::searchInCommunity(
		not_null<Window::SessionController*> controller) {
	const auto channel = _peer->asChannel();
	if (!channel) {
		return;
	}
	const auto history = channel->owner().history(channel);
	controller->hideLayer();
	controller->hideSpecialLayer();
	controller->searchInChat(history);
}

void TopBar::setupActions(not_null<Window::SessionController*> controller) {
	const auto peer = _peer;
	const auto user = peer->asUser();
	const auto channel = peer->asChannel();
	const auto chat = peer->asChat();
	const auto topic = _key.topic();
	const auto sublist = _key.sublist();
	const auto isSide = (_wrap.current() == Wrap::Side);

	if (_source == Source::Community) {
		_actions = base::make_unique_q<Ui::HorizontalFitContainer>(
			this,
			st::infoProfileTopBarActionButtonsSpace);
		auto buttons = std::vector<not_null<TopBarActionButton*>>();
		if (channel && channel->canEditInformation()) {
			const auto manage = Ui::CreateChild<TopBarActionButton>(
				this,
				tr::lng_profile_action_short_manage(tr::now),
				st::infoProfileTopBarActionManage);
			manage->setClickedCallback([=] {
				ShowManageCommunityBox(controller, channel);
			});
			manage->setAccessibleName(
				tr::lng_profile_action_short_manage(tr::now));
			buttons.push_back(manage);
			_actions->add(manage);
		}
		if (channel) {
			const auto search = Ui::CreateChild<TopBarActionButton>(
				this,
				tr::lng_dlg_filter(tr::now),
				st::infoProfileTopBarActionSearch);
			search->setClickedCallback([=] {
				searchInCommunity(controller);
			});
			search->setAccessibleName(tr::lng_dlg_filter(tr::now));
			buttons.push_back(search);
			_actions->add(search);
		}
		finalizeActions(buttons);
		return;
	}

	auto buttons = std::vector<not_null<TopBarActionButton*>>();
	_actions = base::make_unique_q<Ui::HorizontalFitContainer>(
		this,
		st::infoProfileTopBarActionButtonsSpace);
	const auto chechMax = [&, max = 3] {
		return buttons.size() >= max;
	};
	const auto addMore = [&] {
		if ([&]() -> bool {
			if (isSide) {
				return false;
			}
			const auto guard = gsl::finally([&] { _peerMenu = nullptr; });
			showTopBarMenu(controller, true);
			return _peerMenu;
		}()) {
			const auto moreButton = Ui::CreateChild<TopBarActionButton>(
				this,
				tr::lng_profile_action_short_more(tr::now),
				st::infoProfileTopBarActionMore);
			moreButton->setClickedCallback([=] {
				showTopBarMenu(controller, false);
			});
			moreButton->setAccessibleName(tr::lng_profile_action_short_more(tr::now));
			_actionMore = moreButton;
			_actions->add(moreButton);
			buttons.push_back(moreButton);
		}
	};
	const auto guard = gsl::finally([&] {
		addMore();
		finalizeActions(buttons);
	});
	if (user) {
		const auto message = Ui::CreateChild<TopBarActionButton>(
			this,
			tr::lng_profile_action_short_message(tr::now),
			st::infoProfileTopBarActionMessage);
		message->setClickedCallback([=, window = controller] {
			window->showPeerHistory(
				peer->id,
				Window::SectionShow::Way::Forward);
		});
		message->setAccessibleName(tr::lng_profile_action_short_message(tr::now));
		buttons.push_back(message);
		_actions->add(message);
	}
	const auto canJoin = (!sublist && !topic && channel && !channel->amIn());
	if (canJoin) {
		const auto join = Ui::CreateChild<TopBarActionButton>(
			this,
			tr::lng_profile_action_short_join(tr::now),
			st::infoProfileTopBarActionJoin);
		join->setClickedCallback([=] {
			channel->session().api().joinChannel(channel);
		});
		join->setAccessibleName(tr::lng_profile_action_short_join(tr::now));
		buttons.push_back(join);
		_actions->add(join);
	} else if (const auto channel = peer->monoforumBroadcast()) {
		const auto message = Ui::CreateChild<TopBarActionButton>(
			this,
			tr::lng_profile_action_short_channel(tr::now),
			st::infoProfileTopBarActionMessage);
		message->setClickedCallback([=, window = controller] {
			window->showPeerHistory(
				channel,
				Window::SectionShow::Way::Forward);
		});
		message->setAccessibleName(tr::lng_profile_action_short_channel(tr::now));
		buttons.push_back(message);
		_actions->add(message);
	}
	if (!peer->isSelf()) {
		const auto notifications = Ui::CreateChild<TopBarActionButton>(
			this,
			tr::lng_profile_action_short_mute(tr::now),
			st::infoProfileTopBarActionMessage);
		notifications->setAccessibleName(tr::lng_profile_action_short_mute(tr::now));
		notifications->convertToToggle(
			st::infoProfileTopBarActionUnmute,
			st::infoProfileTopBarActionMute,
			u"profile_muting"_q,
			u"profile_unmuting"_q);

		const auto topicRootId = topic ? topic->rootId() : MsgId();
		const auto makeThread = [=] {
			return topicRootId
				? static_cast<Data::Thread*>(peer->forumTopicFor(topicRootId))
				: peer->owner().history(peer).get();
		};
		(topic
			? NotificationsEnabledValue(topic)
			: NotificationsEnabledValue(peer)
		) | rpl::on_next([=](bool enabled) {
			notifications->toggle(enabled);
			const auto text = enabled
				? tr::lng_profile_action_short_mute(tr::now)
				: tr::lng_profile_action_short_unmute(tr::now);
			notifications->setText(text);
			notifications->setAccessibleName(text);
		}, notifications->lifetime());
		notifications->finishAnimating();

		notifications->setAcceptBoth();
		notifications->clicks(
		) | rpl::filter([](Qt::MouseButton button) {
			return (button == Qt::LeftButton);
		}) | rpl::on_next([=] {
			if (const auto thread = makeThread()) {
				MuteMenu::ToggleMuteForever(thread);
			}
		}, notifications->lifetime());
		MuteMenu::SetupMuteMenu(
			notifications,
			notifications->clicks(
			) | rpl::filter([](Qt::MouseButton button) {
				return (button == Qt::RightButton);
			}) | rpl::to_empty,
			makeThread,
			controller->uiShow(),
			[=, skip = st::infoProfileTopBarActionMenuSkip] {
				return notifications->mapToGlobal(
					QPoint(0, notifications->height() + skip));
			});
		buttons.push_back(notifications);
		_actions->add(notifications);
		_edgeColor.value() | rpl::on_next([=](
				std::optional<QColor> c) {
			notifications->setLottieColor(c
				? (const style::color*)(nullptr)
				: &st::windowBoldFg);
		}, notifications->lifetime());
	}
	if (chechMax()) {
		return;
	}
	if (!isSide
		&& user
		&& !user->sharedMediaInfo()
		&& !user->isInaccessible()
		&& user->callsStatus() != UserData::CallsStatus::Disabled) {
		const auto call = Ui::CreateChild<TopBarActionButton>(
			this,
			tr::lng_profile_action_short_call(tr::now),
			st::infoProfileTopBarActionCall);
		call->setClickedCallback([=] {
			Core::App().calls().startOutgoingCall(user, {});
		});
		call->setAccessibleName(tr::lng_profile_action_short_call(tr::now));
		buttons.push_back(call);
		_actions->add(call);
	}
	if (chechMax()) {
		return;
	}
	if (const auto chat = channel ? channel->discussionLink() : nullptr;
			chat && chat->isMegagroup()) {
		const auto discuss = Ui::CreateChild<TopBarActionButton>(
			this,
			tr::lng_profile_action_short_discuss(tr::now),
			st::infoProfileTopBarActionMessage);
		discuss->setClickedCallback([=] {
			if (channel->invitePeekExpires()) {
				controller->showToast(
					tr::lng_channel_invite_private(tr::now));
				return;
			}
			auto params = Window::SectionShow(
				Window::SectionShow::Way::Forward);
			params.preferCurrentWindow = true;
			controller->showPeerHistory(chat, params);
		});
		discuss->setAccessibleName(tr::lng_profile_action_short_discuss(tr::now));
		_actions->add(discuss);
		buttons.push_back(discuss);
	}
	if (chechMax()) {
		return;
	}
	if (topic ? topic->canEdit() : EditPeerInfoBox::Available(peer)) {
		const auto manage = Ui::CreateChild<TopBarActionButton>(
			this,
			tr::lng_profile_action_short_manage(tr::now),
			st::infoProfileTopBarActionManage);
		manage->setClickedCallback([=, window = controller] {
			if (topic) {
				window->show(Box(
					EditForumTopicBox,
					window,
					peer->owner().history(peer),
					topic->rootId()));
			} else {
				window->showEditPeerBox(peer);
			}
		});
		manage->setAccessibleName(tr::lng_profile_action_short_manage(tr::now));
		buttons.push_back(manage);
		_actions->add(manage);
	}
	if (chechMax()) {
		return;
	}
	if (peer->groupCall()
		|| (!peer->isUser() && peer->canManageGroupCall())) {
		const auto broadcast = peer->isBroadcast();
		const auto text = broadcast
			? tr::lng_profile_action_short_live_stream(tr::now)
			: tr::lng_profile_action_short_video_chat(tr::now);
		const auto liveStream = Ui::CreateChild<TopBarActionButton>(
			this,
			text,
			st::infoProfileTopBarActionLiveStream);
		liveStream->setClickedCallback([=] {
			controller->startOrJoinGroupCall(peer);
		});
		liveStream->setAccessibleName(text);
		buttons.push_back(liveStream);
		_actions->add(liveStream);
	}
	if (chechMax()) {
		return;
	}
	if (Ui::CanSendStarGiftTo(peer)) {
		const auto giftButton = Ui::CreateChild<TopBarActionButton>(
			this,
			tr::lng_profile_action_short_gift(tr::now),
			st::infoProfileTopBarActionGift);
		giftButton->setClickedCallback([=] {
			Ui::ShowStarGiftBox(controller, peer);
		});
		giftButton->setAccessibleName(tr::lng_profile_action_short_gift(tr::now));
		_actions->add(giftButton);
		buttons.push_back(giftButton);
	}
	if (chechMax()) {
		return;
	}
	if (!topic
		&& canJoin
		&& ((chat && !chat->amCreator() && !chat->hasAdminRights())
			|| (channel
				&& !channel->amCreator()
				&& !channel->hasAdminRights()))) {
		const auto show = controller->uiShow();
		const auto reportButton = Ui::CreateChild<TopBarActionButton>(
			this,
			tr::lng_profile_action_short_report(tr::now),
			st::infoProfileTopBarActionReport);
		reportButton->setClickedCallback([=] {
			ShowReportMessageBox(show, peer, {}, {});
		});
		reportButton->setAccessibleName(tr::lng_profile_action_short_report(tr::now));
		_actions->add(reportButton);
		buttons.push_back(reportButton);
	}
	if (chechMax()) {
		return;
	}
	if (!canJoin && !topic && !sublist && (chat || channel)) {
		const auto leaveButton = Ui::CreateChild<TopBarActionButton>(
			this,
			tr::lng_profile_action_short_leave(tr::now),
			st::infoProfileTopBarActionLeave);
		leaveButton->setClickedCallback(
			Window::DeleteAndLeaveHandler(controller, peer));
		leaveButton->setAccessibleName(tr::lng_profile_action_short_leave(tr::now));
		_actions->add(leaveButton);
		buttons.push_back(leaveButton);
	}
}

void TopBar::setupUserpicButton(
		not_null<Window::SessionController*> controller) {
	_userpicButton = base::make_unique_q<Userpic>(
		this,
		[=] { return _hasStories; });

	if (_savedMessages) {
		_userpicButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);
		style::PaletteChanged(
		) | rpl::on_next([=] {
			_userpicUniqueKey = InMemoryKey();
			update();
		}, lifetime());
		return;
	}

	const auto openPhoto = [=, peer = _peer] {
		if (const auto id = peer->userpicPhotoId()) {
			if (const auto photo = peer->owner().photo(id); photo->date()) {
				controller->openPhoto(photo, peer);
			}
		}
	};

	_userpicButton->setAcceptBoth(true);

	const auto menu = _userpicButton->lifetime().make_state<
		base::unique_qptr<Ui::PopupMenu>
	>();
	const auto canReport = [=, peer = _peer] {
		if (!peer->hasUserpic()) {
			return false;
		}
		const auto user = peer->asUser();
		if (!user) {
			return false;
		} else if (user->hasPersonalPhoto()
				|| user->isSelf()
				|| user->isInaccessible()
				|| user->isRepliesChat()
				|| user->isVerifyCodes()
				|| (user->botInfo && user->botInfo->canEditInformation)
				|| user->isServiceUser()) {
			return false;
		}
		return true;
	};

	const auto canChangePhoto = [=, peer = _peer] {
		if (_topicIconView) {
			return false;
		}
		if (const auto user = peer->asUser()) {
			return user->isContact()
				&& !user->isSelf()
				&& !user->isInaccessible()
				&& !user->isServiceUser();
		}
		if (const auto chat = peer->asChat()) {
			return chat->canEditInformation();
		}
		if (const auto channel = peer->asChannel()) {
			return channel->canEditInformation();
		}
		return false;
	};

	const auto canSuggestPhoto = [=, peer = _peer] {
		if (const auto user = peer->asUser()) {
			return !user->isSelf()
				&& !user->isBot()
				&& !user->starsPerMessageChecked()
				&& user->owner().history(user)->lastServerMessage();
		}
		return false;
	};

	const auto hasMenu = [=] {
		if (canChangePhoto()) {
			return true;
		}
		if (canSuggestPhoto()) {
			return true;
		}
		if (_hasStories || canReport()) {
			return !!_peer->userpicPhotoId();
		}
		return false;
	};

	const auto invalidate = [=] {
		_userpicUniqueKey = InMemoryKey();
		const auto hasLeftButton = _peer->userpicPhotoId() || _hasStories;
		_userpicButton->setAttribute(
			Qt::WA_TransparentForMouseEvents,
			!hasLeftButton && !hasMenu());
		_userpicButton->setPointerCursor(hasLeftButton);
		updateVideoUserpic();
		_peer->session().downloaderTaskFinished(
		) | rpl::filter([=] {
			return !Ui::PeerUserpicLoading(_userpicView);
		}) | rpl::on_next([=] {
			update();
			_userpicLoadingLifetime.destroy();
		}, _userpicLoadingLifetime);
		Ui::PostponeCall(this, [=] {
			update();
		});
	};

	rpl::single(
		rpl::empty_value()
	) | rpl::then(
		_peer->session().changes().peerFlagsValue(
			_peer,
			Data::PeerUpdate::Flag::Photo
				| Data::PeerUpdate::Flag::FullInfo) | rpl::to_empty
	) | rpl::on_next(invalidate, lifetime());

	if (const auto broadcast = _peer->monoforumBroadcast()) {
		_peer->session().changes().peerFlagsValue(
			broadcast,
			Data::PeerUpdate::Flag::Photo
				| Data::PeerUpdate::Flag::FullInfo
		) | rpl::to_empty | rpl::on_next(invalidate, lifetime());
	}

	using ChosenType = Ui::UserpicButton::ChosenType;

	const auto chosenMediaCallback = [=](ChosenType type) {
		return [=](Editor::ProfileMedia &&media) {
			auto result = Api::PeerPhoto::UserPhoto{
				.image = std::move(media.image),
				.video = std::move(media.video),
			};
			switch (type) {
			case ChosenType::Set:
				_peer->session().api().peerPhoto().upload(
					_peer,
					std::move(result));
				startUploadOverlay();
				break;
			case ChosenType::Suggest:
				_peer->session().api().peerPhoto().suggest(
					_peer,
					std::move(result));
				break;
			}
		};
	};

	const auto choosePhotoCallback = [=](ChosenType type) {
		return [=](QImage &&image) {
			auto result = Api::PeerPhoto::UserPhoto{
				std::move(image),
				0,
				std::vector<QColor>(),
			};
			switch (type) {
			case ChosenType::Set:
				_peer->session().api().peerPhoto().upload(
					_peer,
					std::move(result));
				startUploadOverlay();
				break;
			case ChosenType::Suggest:
				_peer->session().api().peerPhoto().suggest(
					_peer,
					std::move(result));
				break;
			}
		};
	};

	const auto editorData = [=](ChosenType type) {
		const auto user = _peer->asUser();
		const auto name = (user && !user->firstName.isEmpty())
			? user->firstName
			: _peer->name();
		const auto phrase = (type == ChosenType::Suggest)
			? &tr::lng_profile_suggest_sure
			: (user && !user->isSelf() && !_peer->isBot())
			? &tr::lng_profile_set_personal_sure
			: nullptr;
		const auto useForumShape = (_peer->userpicShape()
			== Ui::PeerUserpicShape::Forum);
		return Editor::EditorData{
			.about = (phrase
				? (*phrase)(
					tr::now,
					lt_user,
					tr::bold(name),
					tr::marked)
				: TextWithEntities()),
			.confirm = ((type == ChosenType::Suggest)
				? tr::lng_profile_suggest_button(tr::now)
				: tr::lng_profile_set_photo_button(tr::now)),
			.confirmVideo = ((type == ChosenType::Suggest)
				? tr::lng_profile_suggest_button(tr::now)
				: tr::lng_profile_video_confirm_button(tr::now)),
			.cropType = (useForumShape
				? Editor::EditorData::CropType::RoundedRect
				: Editor::EditorData::CropType::Ellipse),
			.keepAspectRatio = true,
			.forOtherUser = (user && !user->isSelf()),
		};
	};

	const auto chooseFile = [=](ChosenType type) {
		base::call_delayed(
			st::defaultRippleAnimation.hideDuration,
			crl::guard(this, [=] {
				Editor::PrepareProfileMediaFromFile(
					this,
					&controller->window(),
					editorData(type),
					chosenMediaCallback(type));
			}));
	};

	const auto addFromClipboard = [=](
			Ui::PopupMenu *menu,
			ChosenType type,
			tr::phrase<> text) {
		if (const auto data = QGuiApplication::clipboard()->mimeData()) {
			if (data->hasImage()) {
				auto openEditor = crl::guard(this, [=] {
					Editor::PrepareProfilePhoto(
						this,
						&controller->window(),
						editorData(type),
						choosePhotoCallback(type),
						QGuiApplication::clipboard()->image());
				});
				menu->addAction(
					std::move(text)(tr::now),
					std::move(openEditor),
					&st::menuIconPhoto);
			}
		}
	};

	_userpicButton->clicks() | rpl::on_next([=](
			Qt::MouseButton button) {
		if (button == Qt::RightButton && hasMenu()) {
			*menu = base::make_unique_q<Ui::PopupMenu>(
				this,
				st::popupMenuWithIcons);

			if (_peer->userpicPhotoId()) {
				(*menu)->addAction(
					tr::lng_profile_open_photo(tr::now),
					openPhoto,
					&st::menuIconPhoto);
			}

			if (canReport()) {
				(*menu)->addAction(
					tr::lng_profile_report(tr::now),
					[=] {
						controller->show(
							ReportProfilePhotoBox(
								_peer,
								_peer->owner().photo(
									_peer->userpicPhotoId())));
					},
					&st::menuIconReport);
			}

			if (canChangePhoto()) {
				if (!(*menu)->empty()) {
					(*menu)->addSeparator(&st::expandedMenuSeparator);
				}
				const auto isUser = _peer->asUser();
				if (isUser) {
					(*menu)->addAction(
						tr::lng_profile_set_photo_for(tr::now),
						[=] { chooseFile(ChosenType::Set); },
						&st::menuIconPhotoSet);
					addFromClipboard(
						menu->get(),
						ChosenType::Set,
						tr::lng_profile_set_photo_for_from_clipboard);
					if (canSuggestPhoto()) {
						(*menu)->addAction(
							tr::lng_profile_suggest_photo(tr::now),
							[=] {
								chooseFile(
									ChosenType::Suggest);
							},
							&st::menuIconPhotoSuggest);
						addFromClipboard(
								menu->get(),
								ChosenType::Suggest,
								tr::lng_profile_suggest_photo_from_clipboard);
					}
				} else {
					const auto channel = _peer->asChannel();
					const auto isChannel = channel && !channel->isMegagroup();
					(*menu)->addAction(
						isChannel
							? tr::lng_profile_set_photo_for_channel(tr::now)
							: tr::lng_profile_set_photo_for_group(tr::now),
						[=] { chooseFile(ChosenType::Set); },
						&st::menuIconPhotoSet);
					addFromClipboard(
						menu->get(),
						ChosenType::Set,
						tr::lng_profile_set_photo_for_from_clipboard);
				}
				if (controller) {
					const auto done = [=](UserpicBuilder::Result data) {
						auto result = Api::PeerPhoto::UserPhoto{
							base::take(data.image),
							data.id,
							std::move(data.colors),
						};
						_peer->session().api().peerPhoto().upload(
							_peer,
							std::move(result));
					};
					UserpicBuilder::AddEmojiBuilderAction(
						controller,
						menu->get(),
						_peer->session().api().peerPhoto().emojiListValue(isUser
							? Api::PeerPhoto::EmojiListType::Profile
							: Api::PeerPhoto::EmojiListType::Group),
						done,
						_peer->userpicShape()
							== Ui::PeerUserpicShape::Forum);
				}
			}
			if (!(*menu)->empty()) {
				(*menu)->popup(QCursor::pos());
			}
		} else if (button == Qt::LeftButton) {
			if (_uploadOverlay && _uploadOverlay->uploading()) {
				_peer->session().api().peerPhoto().cancelUpload(_peer);
			} else if (_topicIconView && _topic && _topic->iconId()) {
				const auto document = _peer->owner().document(
					_topic->iconId());
				if (const auto sticker = document->sticker()) {
					const auto packName
						= _peer->owner().customEmojiManager().lookupSetName(
							sticker->set.id);
					if (!packName.isEmpty()) {
						const auto text = tr::lng_profile_topic_toast(
							tr::now,
							lt_name,
							tr::link(packName, u"internal:"_q),
							tr::marked);
						const auto weak = base::make_weak(controller);
						controller->showToast(Ui::Toast::Config{
							.text = text,
							.filter = [=, set = sticker->set](
									const ClickHandlerPtr &handler,
									Qt::MouseButton) {
								if (const auto strong = weak.get()) {
									strong->show(
										Box<StickerSetBox>(
											strong->uiShow(),
											set,
											Data::StickersType::Emoji));
								}
								return false;
							},
							.duration = crl::time(3000),
						});
					}
				}
			} else if (_hasStories) {
				controller->openPeerStories(_peer->id);
			} else {
				openPhoto();
			}
		}
	}, _userpicButton->lifetime());
}

void TopBar::startUploadOverlay() {
	if (_uploadOverlay && _uploadOverlay->uploading()) {
		return;
	}
	_waitingUserpicCloudLoad = true;
	_uploadOverlay = std::make_unique<Ui::UploadProgressOverlay>(
		this,
		[=] { update(); });
	_uploadOverlay->start();

	_userpicButton->events(
	) | rpl::filter([](not_null<QEvent*> e) {
		return e->type() == QEvent::Enter
			|| e->type() == QEvent::Leave;
	}) | rpl::on_next([=](not_null<QEvent*> e) {
		if (_uploadOverlay) {
			_uploadOverlay->setOver(e->type() == QEvent::Enter);
		}
	}, _uploadLifetime);

	const auto cleanup = [=] {
		_uploadLifetime.destroy();
		_uploadOverlay = nullptr;
	};
	_peer->session().api().peerPhoto().subscribeToUpload(
		_peer,
		_uploadLifetime,
		{
			.progress = [=](float64 value) {
				_uploadOverlay->setProgress(value);
			},
			.done = [=] {
				_uploadOverlay->stop(cleanup);
			},
			.failed = [=] {
				_uploadOverlay->fail(cleanup);
			},
		});
}

void TopBar::setupUniqueBadgeTooltip() {
	if (!_badge || _source == Source::Preview) {
		return;
	}
	base::timer_once(kWaitBeforeGiftBadge) | rpl::then(
		_badge->updated()
	) | rpl::on_next([=] {
		const auto widget = _badge->widget();
		const auto &content = _badgeContent.current();
		const auto &collectible = content.emojiStatusId.collectible;
		const auto premium = (content.badge == BadgeType::Premium);
		const auto id = (collectible && widget && premium)
			? collectible->id
			: uint64();
		if (_badgeTooltip && _badgeCollectibleId == id) {
			if (widget) {
				_badgeTooltip->trackWidget(widget);
			}
			return;
		}
		hideBadgeTooltip();
		if (!id || _localCollectible) {
			return;
		}
		_badgeTooltip = std::make_unique<Ui::GlareTooltip>(
			this,
			st::infoGiftTooltip,
			st::infoGiftTooltipFont,
			collectible->title,
			Ui::GlareTooltipColors{
				.edge = collectible->edgeColor,
				.center = collectible->centerColor,
				.rim = collectible->textColor,
				.text = QColor(255, 255, 255),
			});
		_badgeCollectibleId = id;
		_badgeTooltip->trackWidget(widget);
		const auto raw = _badgeTooltip.get();
		raw->fade(true);
		_badgeTooltipHide->callOnce(raw->glaresDuration(kGiftBadgeGlares));
		raw->setOpacity(_progress.current());
	}, lifetime());

	if (const auto raw = _badgeTooltip.get()) {
		raw->finishAnimating();
	}
}

void TopBar::hideBadgeTooltip() {
	_badgeTooltipHide->cancel();
	_badgeCollectibleId = 0;
	if (auto old = base::take(_badgeTooltip)) {
		const auto raw = old.get();
		_badgeOldTooltips.push_back(std::move(old));

		raw->fade(false);
		raw->shownValue(
		) | rpl::filter(
			!rpl::mappers::_1
		) | rpl::on_next([=] {
			const auto i = ranges::find(
				_badgeOldTooltips,
				raw,
				&std::unique_ptr<Ui::GlareTooltip>::get);
			if (i != end(_badgeOldTooltips)) {
				_badgeOldTooltips.erase(i);
			}
		}, raw->lifetime());
	}
}

TopBar::~TopBar() {
	base::take(_badgeTooltip);
	base::take(_badgeOldTooltips);
}

rpl::producer<> TopBar::backRequest() const {
	return _backClicks.events();
}

void TopBar::setupSwipeBack(
		not_null<Window::SessionController*> controller) {
	auto update = [=](Ui::Controls::SwipeContextData data) {
		if (data.translation > 0) {
			if (!_swipeBackData.callback) {
				const auto container = static_cast<Ui::RpWidget*>(
					parentWidget());
				_swipeBackData = Ui::Controls::SetupSwipeBack(
					container ? container : static_cast<Ui::RpWidget*>(this),
					[]() -> std::pair<QColor, QColor> {
						return {
							st::historyForwardChooseBg->c,
							st::historyForwardChooseFg->c,
						};
					});
			}
			_swipeBackData.callback(data);
			return;
		} else if (_swipeBackData.lifetime) {
			_swipeBackData = {};
		}
	};

	auto init = [=](Ui::Controls::SwipeHandlerInitData data) {
		if (data.direction != Qt::RightToLeft) {
			return Ui::Controls::SwipeHandlerFinishData();
		}
		auto dismiss = Fn<void()>();
		if (_back && _back->toggled()) {
			dismiss = [=] { _backClicks.fire({}); };
		} else if (_close) {
			dismiss = (_wrap.current() == Wrap::Side)
				? Fn<void()>([=] {
					controller->closeThirdSection();
				})
				: Fn<void()>([=] {
					controller->hideLayer();
					controller->hideSpecialLayer();
				});
		}
		return dismiss
			? Ui::Controls::DefaultSwipeBackHandlerFinishData(
				std::move(dismiss))
			: Ui::Controls::SwipeHandlerFinishData();
	};

	Ui::Controls::SetupSwipeHandler({
		.widget = this,
		.update = std::move(update),
		.init = std::move(init),
	});
}

void TopBar::setOnlineCount(rpl::producer<int> &&count) {
	std::move(count) | rpl::on_next([=](int v) {
		if (_statusLabel) {
			_statusLabel->setOnlineCount(v);
		}
	}, lifetime());
}

void TopBar::setRoundEdges(bool value) {
	_roundEdges = value;
	update();
}

void TopBar::setLottieSingleLoop(bool value) {
	_lottieSingleLoop = value;
}

void TopBar::setColorProfileIndex(std::optional<uint8> index) {
	_localColorProfileIndex = index;
	updateCollectibleStatus();
}

void TopBar::setPatternEmojiId(std::optional<DocumentId> patternEmojiId) {
	_localPatternEmojiId = patternEmojiId;
	updateCollectibleStatus();
}

void TopBar::setLocalEmojiStatusId(EmojiStatusId emojiStatusId) {
	_localCollectible = emojiStatusId.collectible;
	_badgeContent = Badge::Content{ BadgeType::Premium, emojiStatusId };
	updateCollectibleStatus();
}

std::optional<Data::ColorProfileSet> TopBar::effectiveColorProfile() const {
	return _localColorProfileIndex
		? _peer->session().api().peerColors().colorProfileFor(
			*_localColorProfileIndex)
		: _source == Source::Preview
		? std::nullopt
		: _peer->session().api().peerColors().colorProfileFor(_peer);
}

auto TopBar::effectiveCollectible() const
-> std::shared_ptr<Data::EmojiStatusCollectible> {
	return _localCollectible
		? _localCollectible
		: _localColorProfileIndex
		? nullptr
		: _peer->emojiStatusId().collectible;
}

bool TopBar::clipTouchesRoundedCorners(const QRect &clip) const {
	if (!_roundEdges) {
		return false;
	}
	const auto radius = st::boxRadius;
	return (clip.top() < radius)
		&& ((clip.left() < radius) || (clip.right() >= width() - radius));
}

void TopBar::paintEdges(
		QPainter &p,
		const QRect &clip,
		const QBrush &brush) const {
	const auto r = rect();
	if (clipTouchesRoundedCorners(clip)) {
		auto hq = PainterHighQualityEnabler(p);
		const auto radius = st::boxRadius;
		p.setPen(Qt::NoPen);
		p.setBrush(brush);
		p.drawRoundedRect(
			r + QMargins{ 0, 0, 0, radius + 1 },
			radius,
			radius);
	} else {
		p.fillRect(r.intersected(clip), brush);
	}
}

void TopBar::paintEdges(QPainter &p, const QRect &clip) const {
	if (!_solidBg) {
		paintEdges(p, clip, st::boxDividerBg);
	} else {
		paintEdges(p, clip, *_solidBg);
	}
}

int TopBar::titleMostLeft() const {
	return (_back && _back->toggled())
		? _back->width()
		: _st.titleWithSubtitlePosition.x();
}

int TopBar::statusMostLeft() const {
	return (_back && _back->toggled())
		? _back->width()
		: _st.subtitlePosition.x();
}

int TopBar::calculateRightButtonsWidth() const {
	auto width = 0;
	if (_close) {
		width += _close->width();
	}
	if (_topBarButton) {
		width += _topBarButton->width();
	}
	if (_tabMenuToggle && _tabMenuToggle->toggled()) {
		width += _tabMenuToggle->width();
	}
	if (_tabSearchToggle && _tabSearchToggle->toggled()) {
		width += _tabSearchToggle->width();
	}
	if (_tabGroupToggle && _tabGroupToggle->toggled()) {
		width += _tabGroupToggle->width();
	}
	return width;
}

void TopBar::updateLabelsPosition() {
	if (width() <= 0) {
		return;
	}
	_progress = progressForHeight(height());
	const auto progressCurrent = _progress.current();

	updateTitlePosition(progressCurrent);
	updateStatusPosition(progressCurrent);

	if (_badgeTooltip) {
		_badgeTooltip->setOpacity(progressCurrent);
	}

	{
		const auto userpicRect = userpicGeometry();
		if (_userpicButton) {
			_userpicButton->setGeometry(userpicGeometry());
		}

		updateGiftButtonsGeometry(progressCurrent, userpicRect);
	}

	updateRightButtonsPosition();
	updateTabSwapVisibility();
	updateTabSelectionGeometry();
	updateTabSearchGeometry();
}

float64 TopBar::progressForHeight(int height) const {
	const auto max = QWidget::maximumHeight();
	const auto min = _minForProgress;
	const auto p = _savedMessages
		? 0.
		: (max > min)
		? ((height - min) / float64(max - min))
		: 1.;
	return std::clamp(p, 0., 1.);
}

void TopBar::updateTitlePosition(float64 progressCurrent) {
	if (width() <= 0) {
		return;
	}
	const auto rightButtonsWidth = calculateRightButtonsWidth();

	const auto reservedRight = anim::interpolate(
		0,
		rightButtonsWidth,
		1. - progressCurrent);
	const auto titleMostLeft = TopBar::titleMostLeft();
	const auto interpolatedPadding = anim::interpolate(
		titleMostLeft,
		rect::m::sum::h(st::boxRowPadding),
		progressCurrent);
	const auto verifiedWidget = _verified ? _verified->widget() : nullptr;
	const auto badgeWidget = _badge ? _badge->widget() : nullptr;
	const auto botVerifyWidget = _botVerify ? _botVerify->widget() : nullptr;
	auto badgesWidth = 0;
	if (verifiedWidget) {
		badgesWidth += verifiedWidget->width();
	}
	if (badgeWidget) {
		badgesWidth += badgeWidget->width();
	}
	if (botVerifyWidget) {
		badgesWidth += botVerifyWidget->width();
	}
	if (verifiedWidget || badgeWidget) {
		badgesWidth += st::infoVerifiedCheckPosition.x();
	}
	if (verifiedWidget && badgeWidget) {
		badgesWidth += style::ConvertScale(4);
	}
	const auto titleWidth = width()
		- interpolatedPadding
		- reservedRight
		- badgesWidth;

	if (titleWidth > 0) {
		_title->resizeToNaturalWidth(titleWidth);
	}

	const auto titleTop = anim::interpolate(
		_st.titleWithSubtitlePosition.y(),
		st::infoProfileTopBarTitleTop,
		progressCurrent);

	const auto badgeTop = titleTop;
	const auto badgeBottom = titleTop + _title->height();
	const auto margins = LargeCustomEmojiMargins();

	auto totalElementsWidth = _title->width();
	const auto botVerifySkip = botVerifyWidget
		? botVerifyWidget->width() + st::infoVerifiedCheckPosition.x()
		: 0;
	if (verifiedWidget) {
		totalElementsWidth += verifiedWidget->width();
	}
	if (badgeWidget) {
		totalElementsWidth += badgeWidget->width();
	}
	if (verifiedWidget || badgeWidget) {
		totalElementsWidth += st::infoVerifiedCheckPosition.x();
	}
	totalElementsWidth += botVerifySkip;

	auto titleLeft = anim::interpolate(
		titleMostLeft,
		(width() - totalElementsWidth) / 2,
		progressCurrent);

	if (_botVerify) {
		_botVerify->move(
			titleLeft,
			badgeTop,
			badgeBottom);
		titleLeft += margins.left() + botVerifySkip;
	}

	_title->moveToLeft(titleLeft, titleTop);
	const auto badgeLeft = titleLeft + _title->width();
	if (_badge) {
		_badge->move(badgeLeft, badgeTop, badgeBottom);
	}
	if (_verified) {
		const auto gap = (badgeWidget && badgeWidget->width())
			? style::ConvertScale(4)
			: 0;
		_verified->move(
			badgeLeft + (badgeWidget ? badgeWidget->width() : 0) + gap,
			badgeTop,
			badgeBottom);
	}
}

void TopBar::updateStatusPosition(float64 progressCurrent) {
	if (width() <= 0) {
		return;
	}
	if (_forumButton) {
		const auto buttonTop = anim::interpolate(
			_st.subtitlePosition.y(),
			st::infoProfileTopBarStatusTop,
			progressCurrent);
		const auto mostLeft = statusMostLeft();
		const auto buttonMostLeft = anim::interpolate(
			mostLeft,
			st::infoProfileTopBarActionButtonsPadding.left(),
			progressCurrent);
		const auto buttonMostRight = anim::interpolate(
			calculateRightButtonsWidth(),
			st::infoProfileTopBarActionButtonsPadding.right(),
			progressCurrent);
		const auto maxWidth = width() - buttonMostLeft - buttonMostRight;
		if (_forumButton->contentWidth() > maxWidth) {
			_forumButton->setFullWidth(maxWidth);
		}
		const auto buttonLeft = anim::interpolate(
			mostLeft,
			(width() - _forumButton->width()) / 2,
			progressCurrent);
		_forumButton->moveToLeft(buttonLeft, buttonTop);
		_forumButton->setVisible(!tabSwapActive());

		_status->hide();
		// _starsRating->hide();
		_showLastSeen->hide(anim::type::instant);
		return;
	}

	const auto swapProgress = _tabSwapAnimation.value(
		_tabSwapShown ? 1. : 0.);
	_status->setOpacity(1. - swapProgress);
	_status->setVisible(swapProgress < 1.);

	const auto swapShift = int(base::SafeRound(swapProgress
		* statusStyle().style.font->height
		* kSwapMoveAmplitude
		* (_tabSwapShown ? 1. : -1.)));
	const auto statusTop = anim::interpolate(
		_st.subtitlePosition.y(),
		st::infoProfileTopBarStatusTop,
		progressCurrent) + swapShift;
	const auto totalElementsWidth = _status->width()
		+ (_starsRating ? _starsRating->width() : 0)
		+ (_showLastSeen->toggled() ? _showLastSeen->width() : 0);
	const auto statusLeft = anim::interpolate(
		statusMostLeft(),
		(width() - totalElementsWidth) / 2,
		progressCurrent);

	if (const auto rating = _starsRating.get()) {
		rating->moveTo(statusLeft, statusTop - st::lineWidth);
		rating->setOpacity(progressCurrent);
	}
	const auto statusShift = _statusShift.current()
		* std::clamp((progressCurrent) / 0.15, 0., 1.);

	_status->moveToLeft(statusLeft + statusShift, statusTop);
	if (_pills && !_hasActions) {
		_pills->place(
			statusTop - swapShift + st::infoProfileTopBarBalanceSkip,
			progressCurrent);
	}

	if (_showLastSeen->toggled()) {
		_showLastSeen->moveToLeft(
			statusLeft
				+ statusShift
				+ _status->textMaxWidth()
				+ st::infoProfileTopBarLastSeenSkip.x(),
			statusTop + st::infoProfileTopBarLastSeenSkip.y());
		_showLastSeen->setOpacity(progressCurrent);
		_showLastSeen->entity()->setAttribute(
			Qt::WA_TransparentForMouseEvents,
			!progressCurrent);
	}
}

void TopBar::bindActiveTab(
		rpl::producer<TabTopBarBindings> bindings,
		rpl::producer<bool> docked) {
	_tabsDocked = std::move(docked);
	_tabsDocked.changes(
	) | rpl::on_next([=] {
		updateLabelsPosition();
	}, lifetime());
	std::move(
		bindings
	) | rpl::on_next([=](TabTopBarBindings &&value) {
		applyTabBindings(std::move(value));
	}, lifetime());
}

void TopBar::applyTabBindings(TabTopBarBindings &&bindings) {
	_tabBindingsLifetime.destroy();
	_tabBindingsActive = bool(bindings.title);
	if (_tabBindingsActive) {
		if (!_tabSubtitle) {
			_tabSubtitle = std::make_unique<Ui::AnimatedString>(
				statusStyle().style.font,
				[=] { update(); },
				Ui::AnimatedString::Options{
					.splitByWords = true,
					.duration = st::infoTopBarDuration,
				});
			_tabSubtitle->setText(QString(), false);
		}
		if (bindings.subtitle) {
			std::move(
				bindings.subtitle
			) | rpl::on_next([=](const TextWithEntities &text) {
				_tabSubtitleText = text.text;
				refreshTabSubtitle();
			}, _tabBindingsLifetime);
		} else {
			_tabSubtitleText = QString();
			refreshTabSubtitle();
		}
	} else {
		_tabSubtitleText = QString();
		refreshTabSubtitle();
	}
	_tabSelectionAction = std::move(bindings.selectionAction);
	_tabFillMenu = std::move(bindings.fillMenu);
	if (bindings.selectedItems) {
		std::move(
			bindings.selectedItems
		) | rpl::on_next([=](SelectedItems &&items) {
			setTabSelectedItems(std::move(items));
		}, _tabBindingsLifetime);
	} else {
		setTabSelectedItems(SelectedItems());
	}
	_tabApplySearch = std::move(bindings.applySearchQuery);
	_tabSearchAvailable = false;
	hideTabSearch();
	if (bindings.searchEnabledByContent) {
		std::move(
			bindings.searchEnabledByContent
		) | rpl::on_next([=](bool enabled) {
			if (_tabSearchAvailable != enabled) {
				_tabSearchAvailable = enabled;
				updateTabSwapVisibility();
			}
		}, _tabBindingsLifetime);
	}
	_tabSetGroup = std::move(bindings.setGroupByRole);
	_tabGroupActive = false;
	if (bindings.groupByRoleState) {
		std::move(
			bindings.groupByRoleState
		) | rpl::on_next([=](bool grouped) {
			_tabGroupActive = grouped;
			updateTabGroupActive();
			updateTabSwapVisibility();
		}, _tabBindingsLifetime);
	}
	_tabGroupAvailable = false;
	if (bindings.groupByRoleAvailable) {
		std::move(
			bindings.groupByRoleAvailable
		) | rpl::on_next([=](bool available) {
			_tabGroupAvailable = available;
			updateTabSwapVisibility();
		}, _tabBindingsLifetime);
	}
	updateTabGroupActive();
	updateTabSwapVisibility();
}

void TopBar::setupStandaloneGroupControl(
		rpl::producer<bool> state,
		rpl::producer<bool> available,
		rpl::producer<bool> reached,
		Fn<void(bool)> toggle) {
	_standaloneGroup = true;
	_tabSetGroup = std::move(toggle);
	_tabGroupActive = false;
	_tabGroupAvailable = false;
	_standaloneGroupReached = false;
	std::move(
		state
	) | rpl::on_next([=](bool grouped) {
		_tabGroupActive = grouped;
		updateTabGroupActive();
		updateTabSwapVisibility();
	}, lifetime());
	std::move(
		available
	) | rpl::on_next([=](bool value) {
		_tabGroupAvailable = value;
		updateTabSwapVisibility();
	}, lifetime());
	std::move(
		reached
	) | rpl::on_next([=](bool value) {
		_standaloneGroupReached = value;
		updateTabSwapVisibility();
	}, lifetime());
	updateTabGroupActive();
	updateTabSwapVisibility();
}

bool TopBar::tabSelectionMode() const {
	return !_tabSelectedItems.list.empty();
}

void TopBar::setTabSelectedItems(SelectedItems &&items) {
	_tabSelectedItems = std::move(items);
	const auto mode = tabSelectionMode();
	if (mode && !_tabSelectionBar) {
		createTabSelectionBar();
	}
	if (_tabSelectionBar) {
		if (mode) {
			updateTabSelectionState();
			raiseTabSelectionOverlay();
		}
		_tabSelectionBar->toggle(mode, anim::type::normal);
	}
}

void TopBar::raiseTabSelectionOverlay() {
	if (!_tabSelectionBar || !tabSelectionMode()) {
		return;
	}
	_tabSelectionBar->raise();
}

void TopBar::createTabSelectionBar() {
	_tabSelectionBar.create(
		this,
		object_ptr<Ui::RpWidget>(this));
	const auto bar = _tabSelectionBar.data();
	const auto inner = SetupTabBarBackground(bar, _st.bg, _roundEdges);

	const auto forwardAction = [=](SelectionAction action) {
		if (const auto onstack = _tabSelectionAction) {
			onstack(action);
		}
	};
	_tabSelectionCancel = Ui::CreateChild<Ui::IconButton>(
		inner,
		_st.mediaCancel);
	_tabSelectionCancel->setAccessibleName(
		tr::lng_context_clear_selection(tr::now));
	_tabSelectionCancel->clicks(
	) | rpl::on_next([=] {
		forwardAction(SelectionAction::Clear);
	}, _tabSelectionCancel->lifetime());

	_tabSelectionText = Ui::CreateChild<Ui::LabelWithNumbers>(
		inner,
		_st.title,
		_st.titlePosition.y(),
		Ui::StringWithNumbers());
	_tabSelectionText->resize(0, QWidget::minimumHeight());

	_tabSelectionForward = Ui::CreateChild<Ui::IconButton>(
		inner,
		_st.mediaForward);
	_tabSelectionForward->setAccessibleName(
		tr::lng_context_forward_selected(tr::now));
	_tabSelectionForward->clicks(
	) | rpl::on_next([=] {
		forwardAction(SelectionAction::Forward);
	}, _tabSelectionForward->lifetime());

	_tabSelectionDelete = Ui::CreateChild<Ui::IconButton>(
		inner,
		_st.mediaDelete);
	_tabSelectionDelete->setAccessibleName(
		tr::lng_context_delete_selected(tr::now));
	_tabSelectionDelete->clicks(
	) | rpl::on_next([=] {
		forwardAction(SelectionAction::Delete);
	}, _tabSelectionDelete->lifetime());

	_tabSelectionStoryInProfile = Ui::CreateChild<Ui::IconButton>(
		inner,
		_st.storiesSave);
	_tabSelectionStoryInProfile->clicks(
	) | rpl::on_next([=] {
		const auto allInProfile = ranges::all_of(
			_tabSelectedItems.list,
			&SelectedItem::storyInProfile);
		forwardAction(allInProfile
			? SelectionAction::ToggleStoryToArchive
			: SelectionAction::ToggleStoryToProfile);
	}, _tabSelectionStoryInProfile->lifetime());

	_tabSelectionStoryPin = Ui::CreateChild<Ui::IconButton>(
		inner,
		_st.storiesPin);
	_tabSelectionStoryPin->clicks(
	) | rpl::on_next([=] {
		forwardAction(SelectionAction::ToggleStoryPin);
	}, _tabSelectionStoryPin->lifetime());

	_tabSelectionCancel->show();
	_tabSelectionText->show();
	updateTabSelectionGeometry();
	bar->raise();
}

void TopBar::updateTabSelectionState() {
	Expects(_tabSelectionBar != nullptr);

	const auto &list = _tabSelectedItems.list;
	const auto canDelete = ranges::all_of(list, &SelectedItem::canDelete);
	const auto canForward = ranges::all_of(list, &SelectedItem::canForward);
	const auto canToggleStoryPin = ranges::all_of(
		list,
		&SelectedItem::canToggleStoryPin);
	const auto allInProfile = ranges::all_of(
		list,
		&SelectedItem::storyInProfile);
	const auto canUnpin = ranges::any_of(
		list,
		&SelectedItem::canUnpinStory);
	_tabSelectionText->setValue(_tabSelectedItems.title
		? _tabSelectedItems.title(int(list.size()))
		: Ui::StringWithNumbers());
	_tabSelectionForward->setVisible(canForward);
	_tabSelectionDelete->setVisible(canDelete);
	_tabSelectionStoryInProfile->setVisible(canToggleStoryPin);
	_tabSelectionStoryInProfile->setIconOverride(
		(allInProfile
			? &_st.storiesArchive.icon
			: &_st.storiesSave.icon),
		(allInProfile
			? &_st.storiesArchive.iconOver
			: &_st.storiesSave.iconOver));
	_tabSelectionStoryInProfile->setAccessibleName(allInProfile
		? tr::lng_mediaview_archive_story(tr::now)
		: tr::lng_mediaview_save_to_profile(tr::now));
	_tabSelectionStoryPin->setVisible(canToggleStoryPin);
	_tabSelectionStoryPin->setIconOverride(
		canUnpin ? &_st.storiesUnpin.icon : nullptr,
		canUnpin ? &_st.storiesUnpin.iconOver : nullptr);
	_tabSelectionStoryPin->setAccessibleName(canUnpin
		? tr::lng_context_unpin_from_top(tr::now)
		: tr::lng_context_pin_to_top(tr::now));
	updateTabSelectionGeometry();
}

void TopBar::updateTabSelectionGeometry() {
	if (!_tabSelectionBar || width() <= 0) {
		return;
	}
	const auto barHeight = QWidget::minimumHeight();
	const auto inner = _tabSelectionBar->entity();
	inner->resize(width(), barHeight);
	_tabSelectionBar->move(0, 0);

	_tabSelectionCancel->moveToLeft(0, 0);
	auto right = _st.mediaActionsSkip;
	if (!_tabSelectionDelete->isHidden()) {
		_tabSelectionDelete->moveToRight(right, 0, inner->width());
		right += _tabSelectionDelete->width();
	}
	if (!_tabSelectionStoryInProfile->isHidden()) {
		_tabSelectionStoryInProfile->moveToRight(right, 0, inner->width());
		right += _tabSelectionStoryInProfile->width();
	}
	if (!_tabSelectionStoryPin->isHidden()) {
		_tabSelectionStoryPin->moveToRight(right, 0, inner->width());
		right += _tabSelectionStoryPin->width();
	}
	if (!_tabSelectionForward->isHidden()) {
		_tabSelectionForward->moveToRight(right, 0, inner->width());
		right += _tabSelectionForward->width();
	}
	const auto left = _tabSelectionCancel->width();
	const auto available = inner->width() - left - right;
	if (available > 0) {
		_tabSelectionText->resizeToNaturalWidth(available);
		_tabSelectionText->moveToLeft(left, 0);
	}
}

void TopBar::showTabSearch() {
	if (!_tabSearchBar) {
		_tabSearchBar.create(
			this,
			object_ptr<Ui::RpWidget>(this));
		const auto inner = SetupTabBarBackground(
			_tabSearchBar.data(),
			_st.bg,
			_roundEdges);

		_tabSearchField = Ui::CreateChild<Ui::InputField>(
			inner,
			_st.searchRow.field,
			tr::lng_dlg_filter());
		_tabSearchField->changes(
		) | rpl::on_next([=] {
			if (const auto onstack = _tabApplySearch) {
				onstack(_tabSearchField->getLastText());
			}
		}, _tabSearchField->lifetime());

		const auto cancel = Ui::CreateChild<Ui::IconButton>(
			inner,
			st::infoTopBarBlackClose);
		cancel->setAccessibleName(tr::lng_sr_cancel_search(tr::now));
		cancel->show();
		cancel->addClickHandler([=] {
			cancelTabSearch();
		});
		inner->widthValue(
		) | rpl::on_next([=](int newWidth) {
			cancel->moveToRight(0, 0, newWidth);
		}, cancel->lifetime());

		_tabSearchField->show();
	}
	_tabSearchShown = true;
	updateTabSwapVisibility();
	updateTabSearchGeometry();
	raiseTabSearchOverlay();
	_tabSearchBar->toggle(true, anim::type::normal);
	_tabSearchField->setFocus();
}

void TopBar::hideTabSearch() {
	if (!_tabSearchBar || !_tabSearchShown) {
		_tabSearchShown = false;
		return;
	}
	_tabSearchShown = false;
	if (_back) {
		Backdrop(_back->entity())->setBackdropColor(buttonsColorOverride());
	}
	if (_tabSearchField->hasFocus()) {
		setFocus();
	}
	_tabSearchField->setText(QString());
	_tabSearchBar->toggle(false, anim::type::normal);
}

bool TopBar::cancelTabSearch() {
	if (!_tabSearchShown) {
		return false;
	} else if (!_tabSearchField->getLastText().isEmpty()) {
		_tabSearchField->setText(QString());
	} else {
		hideTabSearch();
		updateTabSwapVisibility();
	}
	return true;
}

void TopBar::checkBeforeCloseByEscape(Fn<void()> close) {
	if (!cancelTabSearch()) {
		close();
	}
}

bool TopBar::searchAvailable() const {
	return _tabSearchShown || (tabSwapActive() && _tabSearchAvailable);
}

void TopBar::showSearch() {
	if (_tabSearchShown) {
		_tabSearchField->setFocus();
	} else if (tabSwapActive() && _tabSearchAvailable) {
		showTabSearch();
	}
}

void TopBar::raiseTabSearchOverlay() {
	if (!_tabSearchBar || !_tabSearchShown) {
		return;
	}
	_tabSearchBar->raise();
	if (_back) {
		_back->raise();
		Backdrop(_back->entity())->setBackdropColor(std::nullopt);
	}
}

void TopBar::updateTabSearchGeometry() {
	if (!_tabSearchBar || width() <= 0) {
		return;
	}
	const auto barHeight = QWidget::minimumHeight();
	const auto inner = _tabSearchBar->entity();
	inner->resize(width(), barHeight);
	_tabSearchBar->move(0, 0);

	const auto fieldLeft = titleMostLeft();
	const auto fieldWidth = width()
		- fieldLeft
		- _st.searchRow.fieldCancelSkip;
	if (fieldWidth > 0) {
		_tabSearchField->resize(fieldWidth, _tabSearchField->height());
		_tabSearchField->moveToLeft(
			fieldLeft,
			(barHeight - _tabSearchField->height()) / 2);
	}
}

void TopBar::refreshTabSubtitle() {
	if (!_tabSubtitle) {
		return;
	}
	_tabSubtitle->setText(tabSwapActive()
		? _tabSubtitleText
		: QString());
	update();
}

void TopBar::paintTabSubtitle(QPainter &p) {
	if (!_tabSubtitle) {
		return;
	}
	const auto color = _tabSubtitleOverride
		? *_tabSubtitleOverride
		: statusStyle().textFg->c;
	_tabSubtitle->draw(
		p,
		statusMostLeft(),
		_st.subtitlePosition.y(),
		color);
}

bool TopBar::tabSwapActive() const {
	return _tabBindingsActive && _tabsDocked.current();
}

void TopBar::updateTabSwapVisibility() {
	const auto swap = tabSwapActive();
	if (!swap) {
		hideTabSearch();
	}
	auto togglesChanged = false;
	if (_tabMenuToggle) {
		const auto shown = swap
			&& !_tabSearchShown
			&& (_tabFillMenu != nullptr);
		if (_tabMenuToggle->toggled() != shown) {
			_tabMenuToggle->toggle(shown, anim::type::normal);
			togglesChanged = true;
		}
	}
	if (_tabSearchToggle) {
		const auto shown = swap
			&& !_tabSearchShown
			&& _tabSearchAvailable;
		if (_tabSearchToggle->toggled() != shown) {
			_tabSearchToggle->toggle(shown, anim::type::normal);
			togglesChanged = true;
		}
	}
	if (_tabGroupToggle) {
		const auto reached = _standaloneGroup
			? _standaloneGroupReached
			: swap;
		const auto shown = reached
			&& !_tabSearchShown
			&& (_tabSetGroup != nullptr)
			&& _tabGroupAvailable;
		if (_tabGroupToggle->toggled() != shown) {
			if (shown) {
				updateTabGroupActive();
			}
			_tabGroupToggle->toggle(shown, anim::type::normal);
			togglesChanged = true;
		}
	}
	if (togglesChanged) {
		updateRightButtonsPosition();
		updateTitlePosition(_progress.current());
		updateStatusPosition(_progress.current());
	}
	if (!_tabSubtitle && !swap) {
		return;
	}
	if (_tabSwapShown != swap) {
		_tabSwapShown = swap;
		_tabSwapAnimation.start(
			[=] { applyTabSwapProgress(_tabSwapAnimation.value(
				_tabSwapShown ? 1. : 0.)); },
			swap ? 0. : 1.,
			swap ? 1. : 0.,
			st::infoTopBarDuration,
			anim::easeOutQuint);
		refreshTabSubtitle();
	}
	applyTabSwapProgress(
		_tabSwapAnimation.value(_tabSwapShown ? 1. : 0.));
}

void TopBar::updateRightButtonsPosition() {
	if (width() <= 0) {
		return;
	}
	auto right = 0;
	if (_close) {
		_close->moveToRight(right, 0);
		right += _close->width();
	}
	if (_topBarButton) {
		_topBarButton->moveToRight(right, 0);
		right += _topBarButton->width();
	}
	if (_tabMenuToggle && _tabMenuToggle->toggled()) {
		_tabMenuToggle->moveToRight(right, 0);
		right += _tabMenuToggle->width();
	}
	if (_tabSearchToggle && _tabSearchToggle->toggled()) {
		_tabSearchToggle->moveToRight(right, 0);
		right += _tabSearchToggle->width();
	}
	if (_tabGroupToggle && _tabGroupToggle->toggled()) {
		_tabGroupToggle->moveToRight(right, 0);
		right += _tabGroupToggle->width();
	}
}

void TopBar::updateTabGroupActive() {
	if (!_tabGroupToggle) {
		return;
	}
	const auto entity = _tabGroupToggle->entity();
	entity->setForceRippled(false, anim::type::instant);
	entity->setForceRippled(_tabGroupActive, anim::type::instant);
}

void TopBar::applyTabSwapProgress(float64 progress) {
	if (!_forumButton) {
		_status->setOpacity(1. - progress);
		_status->setVisible(progress < 1.);
		updateStatusPosition(_progress.current());
	}
	update();
}

void TopBar::resizeEvent(QResizeEvent *e) {
	_cachedClipPath = QPainterPath();
	const auto collectible = effectiveCollectible();
	if (collectible && !_animatedPoints.empty()) {
		setupAnimatedPattern();
	}
	if (_hasGradientBg && e->oldSize().width() != e->size().width()) {
		_cachedClipPath = QPainterPath();
		_cachedGradient = QImage();
	}
	updateLabelsPosition();
	RpWidget::resizeEvent(e);
}

QRect TopBar::userpicGeometry() const {
	constexpr auto kMinScale = 0.25;
	const auto progressCurrent = _progress.current();
	const auto fullSize = st::infoProfileTopBarPhotoSize;
	const auto minSize = fullSize * kMinScale;
	const auto size = anim::interpolate(minSize, fullSize, progressCurrent);
	const auto x = (width() - size) / 2;
	const auto minY = -minSize;
	const auto maxY = st::infoProfileTopBarPhotoTop;
	const auto y = anim::interpolate(minY, maxY, progressCurrent);
	return QRect(x, y, size, size);
}

void TopBar::updateGiftButtonsGeometry(
		float64 progressCurrent,
		const QRect &userpicRect) {
	if (width() <= 0) {
		return;
	}
	const auto v2Bubbles = profileV2Enabled()
		&& Extera::Bool(u"profile_v2_gift_bubbles"_q)
		&& (_source != Source::Preview);
	const auto sz = v2Bubbles
		? int(std::ceil(GiftBubbleRadius() * 2.))
		: st::infoProfileTopBarGiftSize;
	const auto halfSz = sz / 2.;
	for (const auto &gift : _pinnedToTopGifts) {
		if (gift.button) {
			const auto giftPos = (v2Bubbles && gift.physInited && progressCurrent > 0.25)
				? gift.physPos
				: calculateGiftPosition(
					gift.position,
					progressCurrent,
					userpicRect);
			const auto buttonRect = QRect(
				QPoint(
					int(std::round(giftPos.x() - halfSz)),
					int(std::round(giftPos.y() - halfSz))),
				Size(sz));
			gift.button->setGeometry(buttonRect);
		}
	}
}

void TopBar::paintUserpic(QPainter &p, const QRect &geometry) {
	if (_topicIconView) {
		_topicIconView->paintInRect(p, geometry);
		return;
	}
	if (_savedMessages) {
		const auto key = InMemoryKey(1, 1);
		if (_userpicUniqueKey != key) {
			_userpicUniqueKey = key;
			_cachedUserpic = Ui::EmptyUserpic::GenerateSavedMessages(
				st::infoProfileTopBarPhotoSize * style::DevicePixelRatio());
			_cachedUserpic.setDevicePixelRatio(style::DevicePixelRatio());
		}
		auto hq = PainterHighQualityEnabler(p);
		p.drawImage(geometry, _cachedUserpic);
		return;
	}
	if (_videoUserpicPlayer && _videoUserpicPlayer->ready()) {
		const auto size = st::infoProfileTopBarPhotoSize;
		const auto paused = _gifPausedChecker();
		const auto frame = _videoUserpicPlayer->frame(
			Size(size),
			_peer,
			paused);
		if (!frame.isNull()) {
			auto hq = PainterHighQualityEnabler(p);
			const auto musicScale = (profileV2Enabled()
				&& Extera::Bool(u"profile_v2_music_sync"_q)
				&& _v2MusicEnergy > 0.005)
				? (1. + _v2MusicEnergy * 0.042)
				: 1.;
			if (musicScale > 1.) {
				const auto center = QRectF(geometry).center();
				p.save();
				p.translate(center);
				p.scale(musicScale, musicScale);
				p.translate(-center);
				p.drawImage(geometry, frame);
				p.restore();
			} else {
				p.drawImage(geometry, frame);
			}
			return;
		}
	}
	const auto key = _peer->userpicUniqueKey(_userpicView);
	const auto overlayActive = _uploadOverlay && _uploadOverlay->shown();
	const auto awaitingCloud = _waitingUserpicCloudLoad
		&& !_peer->userpicCloudImage(_userpicView);
	if (!overlayActive && !awaitingCloud && _userpicUniqueKey != key) {
		_waitingUserpicCloudLoad = false;
		_userpicUniqueKey = key;
		const auto fullSize = st::infoProfileTopBarPhotoSize;
		const auto scaled = fullSize * style::DevicePixelRatio();
		auto image = QImage();
		if (const auto broadcast = _peer->monoforumBroadcast()) {
			image = PeerData::GenerateUserpicImage(
				broadcast,
				_userpicView,
				scaled,
				0);
			if (_monoforumMask.isNull()) {
				_monoforumMask = Ui::MonoforumShapeMask(Size(scaled));
			}
			constexpr auto kFormat = QImage::Format_ARGB32_Premultiplied;
			if (image.format() != kFormat) {
				image = std::move(image).convertToFormat(kFormat);
			}
			auto q = QPainter(&image);
			q.setCompositionMode(QPainter::CompositionMode_DestinationIn);
			q.drawImage(
				Rect(image.size() / image.devicePixelRatio()),
				_monoforumMask);
			q.end();
		} else {
			const auto radius = (_source == Source::Community)
				? std::optional<int>(
					int(scaled * Ui::ForumUserpicRadiusMultiplier()))
				: std::nullopt;
			image = PeerData::GenerateUserpicImage(
				_peer,
				_userpicView,
				scaled,
				radius);
		}
		_cachedUserpic = std::move(image);
		_cachedUserpic.setDevicePixelRatio(style::DevicePixelRatio());
	}
	if (!_cachedUserpic.isNull() && _adaptiveAvatarKey != _userpicUniqueKey) {
		extractAdaptiveAvatarColors();
	}
	if (_communityEffect) {
		Ui::PaintCommunityUserpicEffect(
			p,
			_communityUserpicEffect,
			geometry.x(),
			geometry.y(),
			geometry.width(),
			st::windowSubTextFg->c);
	}
	{
		auto hq = PainterHighQualityEnabler(p);
		const auto musicScale = (profileV2Enabled()
			&& Extera::Bool(u"profile_v2_music_sync"_q)
			&& _v2MusicEnergy > 0.005)
			? (1. + _v2MusicEnergy * 0.042)
			: 1.;
		if (musicScale > 1.) {
			const auto center = QRectF(geometry).center();
			p.save();
			p.translate(center);
			p.scale(musicScale, musicScale);
			p.translate(-center);
			p.drawImage(geometry, _cachedUserpic);
			p.restore();
		} else {
			p.drawImage(geometry, _cachedUserpic);
		}
	}
	if (_uploadOverlay && _uploadOverlay->shown()) {
		_uploadOverlay->paint(p, geometry, {
			.lineWidth = st::defaultUserpicButton.uploadProgressLine,
			.margin = st::defaultUserpicButton.uploadProgressMargin,
			.progressFg = st::historyFileThumbRadialFg,
			.overlayFg = st::songCoverOverlayFg,
			.cancelIcon = &st::userpicUploadCancel,
		});
	}
}

void TopBar::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	const auto geometry = userpicGeometry();
	const auto clipBounds = e->region().boundingRect();
	const auto v2 = profileV2Enabled() && (_source != Source::Preview);
	const auto v2Bubbles = v2 && Extera::Bool(u"profile_v2_gift_bubbles"_q);

	if (_hasGradientBg && _cachedGradient.isNull()) {
		const auto collectible = effectiveCollectible();
		const auto colorProfile = effectiveColorProfile();
		const auto shift = _pillsReserve.current() / 2 + (_hasActions
			? st::infoProfileTopBarPhotoBgShift
			: st::infoProfileTopBarPhotoBgNoActionsShift);
		const auto offset = QPoint(0, -shift);
		if (collectible) {
			_cachedGradient = Ui::CreateTopBgGradient(
				QSize(width(), maximumHeight()),
				collectible->centerColor,
				collectible->edgeColor,
				false,
				offset);
		} else if (colorProfile && colorProfile->bg.size() > 1) {
			_cachedGradient = Ui::CreateTopBgGradient(
				QSize(width(), maximumHeight()),
				colorProfile->bg[1],
				colorProfile->bg[0],
				false,
				offset);
		} else if (useAdaptiveAvatarBg()
			&& _adaptiveCenterColor.isValid()
			&& _adaptiveEdgeColor.isValid()) {
			_cachedGradient = Ui::CreateTopBgGradient(
				QSize(width(), maximumHeight()),
				_adaptiveCenterColor,
				_adaptiveEdgeColor,
				false,
				offset);
		}
	}
	if (!_hasGradientBg || _cachedGradient.isNull()) {
		paintEdges(p, clipBounds);
	} else {
		const auto x = (width()
			- _cachedGradient.width() / style::DevicePixelRatio())
				/ 2;
		const auto y = (height()
			- _cachedGradient.height() / style::DevicePixelRatio())
				/ 2;
		if (clipTouchesRoundedCorners(clipBounds)) {
			if (_cachedClipPath.isEmpty()) {
				const auto radius = st::boxRadius;
				_cachedClipPath.addRoundedRect(
					rect() + QMargins{ 0, 0, 0, radius + 1 },
					radius,
					radius);
			}
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(Qt::NoPen);
			p.setClipPath(_cachedClipPath);
			p.drawImage(x, y, _cachedGradient);
		} else {
			p.drawImage(x, y, _cachedGradient);
		}
	}
	if (v2 && !_savedMessages) {
		paintProfileV2Backdrop(p, clipBounds, geometry);
	}
	p.setClipping(false);
	if (_patternEmoji && _patternEmoji->ready()) {
		paintAnimatedPattern(p, clipBounds, geometry);
	}

	if (v2Bubbles) {
		if (!_pinnedToTopGifts.empty()) {
			paintPinnedToTopGifts(p, clipBounds, geometry);
		}
	} else if (clipBounds.bottom() >= geometry.top()
		&& clipBounds.top() <= geometry.bottom()) {
		paintPinnedToTopGifts(p, clipBounds, geometry);
	}

	const auto avatarBounds = v2
		? geometry.adjusted(-14, -14, 14, 14)
		: geometry;
	if (clipBounds.intersects(avatarBounds)) {
		paintUserpic(p, geometry);
		if (v2 && !_savedMessages) {
			paintProfileV2AvatarRim(p, geometry);
		}
		paintStoryOutline(p, geometry);
	}

	if (v2Bubbles && !_pinnedToTopGifts.empty()) {
		const auto progress = _giftsHiding
			? _progress.current() * _giftsHiding->value(1.)
			: (_giftsAppearing
				? _progress.current() * _giftsAppearing->value(0.)
				: _progress.current());
		if (progress > 0.) {
			for (auto &gift : _pinnedToTopGifts) {
				if (gift.physDragging) {
					paintSingleGiftBubble(p, gift, progress, clipBounds, geometry);
				}
			}
			p.setOpacity(1.);
		}
	}

	paintTabSubtitle(p);

	if (v2 && _v2AnimTimer && !_v2AnimTimer->isActive()) {
		updateProfileV2AnimState();
	}
}

void TopBar::setupButtons(
		not_null<Window::SessionController*> controller,
		Source source) {
	if (source == Source::Preview) {
		setRoundEdges(false);
		return;
	}
	rpl::combine(
		_wrap.value(),
		_edgeColor.value()
	) | rpl::on_next([=](
			Wrap wrap,
			std::optional<QColor>) mutable {
		const auto isLayer = (wrap == Wrap::Layer);
		const auto isSide = (wrap == Wrap::Side);
		setRoundEdges(isLayer);
		setLottieSingleLoop(wrap == Wrap::Side);

		_back = base::make_unique_q<Ui::FadeWrap<Ui::IconButton>>(
			this,
			object_ptr<BackdropIconButton>(
				this,
				(isLayer
					? st::infoLayerTopBarBlackBack
					: st::infoTopBarBlackBack)),
			st::infoTopBarScale);
		_back->QWidget::show();
		_back->setDuration(0);
		_back->entity()->setAccessibleName(tr::lng_go_back(tr::now));
		_back->toggleOn(isLayer || isSide
			? (_backToggles.value() | rpl::type_erased)
			: rpl::single(wrap == Wrap::Narrow));
		_back->entity()->clicks() | rpl::to_empty | rpl::start_to_stream(
			_backClicks,
			_back->lifetime());

		if (!isLayer && !isSide) {
			_close = nullptr;
		} else {
			_close = base::make_unique_q<BackdropIconButton>(
				this,
				(isLayer
					? st::infoLayerTopBarBlackClose
					: st::infoTopBarBlackClose));
			_close->setAccessibleName(tr::lng_sr_close_panel(tr::now));
			_close->show();
			_close->addClickHandler(isSide
				? Fn<void()> ([=] { controller->closeThirdSection(); })
				: Fn<void()> ([=] {
					controller->hideLayer();
					controller->hideSpecialLayer();
				}));
			widthValue() | rpl::on_next([=] {
				_close->moveToRight(0, 0);
			}, _close->lifetime());
		}

		_tabMenuToggle = base::make_unique_q<Ui::FadeWrap<Ui::IconButton>>(
			this,
			object_ptr<BackdropIconButton>(
				this,
				st::infoTopBarBlackMenu),
			st::infoTopBarScale);
		_tabMenuToggle->QWidget::show();
		_tabMenuToggle->setDuration(st::infoTopBarDuration);
		_tabMenuToggle->toggle(false, anim::type::instant);
		_tabMenuToggle->entity()->setAccessibleName(
			tr::lng_profile_action_short_more(tr::now));
		_tabMenuToggle->entity()->addClickHandler([=] {
			showTopBarMenu(controller, false);
		});

		_tabSearchToggle = base::make_unique_q<Ui::FadeWrap<Ui::IconButton>>(
			this,
			object_ptr<BackdropIconButton>(
				this,
				st::infoTopBarBlackSearch),
			st::infoTopBarScale);
		_tabSearchToggle->QWidget::show();
		_tabSearchToggle->setDuration(st::infoTopBarDuration);
		_tabSearchToggle->toggle(false, anim::type::instant);
		_tabSearchToggle->entity()->setAccessibleName(
			tr::lng_dlg_filter(tr::now));
		_tabSearchToggle->entity()->addClickHandler([=] {
			showTabSearch();
		});

		_tabGroupToggle = base::make_unique_q<Ui::FadeWrap<Ui::IconButton>>(
			this,
			object_ptr<BackdropIconButton>(
				this,
				st::infoTopBarBlackGroup),
			st::infoTopBarScale);
		_tabGroupToggle->QWidget::show();
		_tabGroupToggle->setDuration(st::infoTopBarDuration);
		_tabGroupToggle->toggle(false, anim::type::instant);
		_tabGroupToggle->entity()->setAccessibleName(
			tr::lng_profile_participants_section(tr::now));
		_tabGroupToggle->entity()->addClickHandler([=] {
			if (_tabSetGroup) {
				_tabSetGroup(!_tabGroupActive);
			}
		});
		_tabGroupToggle->entity()->shownValue(
		) | rpl::filter([](bool shown) {
			return shown;
		}) | rpl::on_next([=] {
			updateTabGroupActive();
		}, _tabGroupToggle->lifetime());
		updateTabGroupActive();

		widthValue() | rpl::on_next([=] {
			updateRightButtonsPosition();
		}, _tabSearchToggle->lifetime());
		updateTabSwapVisibility();
		updateRightButtonsPosition();

		if (wrap != Wrap::Side) {
			if (source == Source::Stories) {
				addTopBarEditButton(controller, wrap);
			}
		}
		updateButtonsColorOverride();
		raiseTabSearchOverlay();
		raiseTabSelectionOverlay();
	}, lifetime());
}

void TopBar::addTopBarEditButton(
		not_null<Window::SessionController*> controller,
		Wrap wrap) {
	_topBarButton = base::make_unique_q<BackdropIconButton>(
		this,
		((wrap == Wrap::Layer)
			? st::infoLayerTopBarBlackEdit
			: st::infoTopBarBlackEdit));
	_topBarButton->show();
	_topBarButton->addClickHandler([=] {
		controller->showSettings(::Settings::InformationId());
	});
	_topBarButton->setAccessibleName(tr::lng_settings_information(tr::now));

	widthValue() | rpl::on_next([=] {
		if (_close) {
			_topBarButton->moveToRight(_close->width(), 0);
		} else {
			_topBarButton->moveToRight(0, 0);
		}
	}, _topBarButton->lifetime());
}

std::optional<QColor> TopBar::buttonsColorOverride() const {
	const auto edgeColor = _edgeColor.current();
	return edgeColor
		? std::make_optional(ContentColorOverBackground(*edgeColor))
		: std::nullopt;
}

void TopBar::updateButtonsColorOverride() {
	const auto color = buttonsColorOverride();
	const auto apply = [&](Ui::IconButton *button) {
		if (button) {
			Backdrop(button)->setBackdropColor(color);
		}
	};
	apply(_back ? _back->entity() : nullptr);
	apply(_close.get());
	apply(_tabMenuToggle ? _tabMenuToggle->entity() : nullptr);
	apply(_tabSearchToggle ? _tabSearchToggle->entity() : nullptr);
	apply(_tabGroupToggle ? _tabGroupToggle->entity() : nullptr);
	apply(_topBarButton.get());
}

void TopBar::showTopBarMenu(
		not_null<Window::SessionController*> controller,
		bool check) {
	if (_peerMenu) {
		_peerMenu->hideMenu(true);
		return;
	}
	_peerMenu = base::make_unique_q<Ui::PopupMenu>(
		QWidget::window(),
		st::popupMenuExpandedSeparator);

	_peerMenu->setDestroyedCallback([this] {
		InvokeQueued(this, [this] { _peerMenu = nullptr; });
		// if (auto toggle = _topBarMenuToggle.get()) {
		// 	toggle->setForceRippled(false);
		// }
	});

	if (tabSwapActive()) {
		if (_tabFillMenu) {
			_tabFillMenu(Ui::Menu::CreateAddActionCallback(_peerMenu));
		}
	} else {
		fillTopBarMenu(
			controller,
			Ui::Menu::CreateAddActionCallback(_peerMenu));
	}
	if (_peerMenu->empty()) {
		_peerMenu = nullptr;
		return;
	} else if (check) {
		return;
	}
	_peerMenu->setForcedOrigin(Ui::PanelAnimation::Origin::TopRight);
	const auto tabToggle = (_tabMenuToggle && _tabMenuToggle->toggled())
		? _tabMenuToggle->entity()
		: nullptr;
	const auto anchor = tabToggle
		? static_cast<QWidget*>(tabToggle)
		: static_cast<QWidget*>(_actionMore);
	const auto extend = Ui::BoxShadow::ExtendFor(_peerMenu->st().shadow);
	const auto skip = tabToggle
		? extend.top()
		: st::infoProfileTopBarActionMenuSkip;
	_peerMenu->popup(anchor
		? Ui::PopupMenu::ConstrainToParentScreen(
			_peerMenu,
			anchor->mapToGlobal(QPoint(
				anchor->width() + extend.right(),
				anchor->height() + skip)))
		: QCursor::pos());
}

void TopBar::fillTopBarMenu(
		not_null<Window::SessionController*> controller,
		const Ui::Menu::MenuCallback &addAction) {
	const auto peer = _peer;
	const auto topic = _key.topic();
	const auto sublist = _key.sublist();
	if (!peer && !topic) {
		return;
	}

	Window::FillDialogsEntryMenu(
		controller,
		Dialogs::EntryState{
			.key = (topic
				? Dialogs::Key{ topic }
				: sublist
				? Dialogs::Key{ sublist }
				: Dialogs::Key{ peer->owner().history(peer) }),
			.section = Dialogs::EntryState::Section::Profile,
		},
		addAction);
}

void TopBar::updateVideoUserpic() {
	const auto id = _peer->userpicPhotoId();
	if (!id) {
		_videoUserpicPlayer = nullptr;
		return;
	}
	const auto photo = _peer->owner().photo(id);
	if (!photo->date() || !photo->videoCanBePlayed()) {
		_videoUserpicPlayer = nullptr;
		return;
	}
	if (!_videoUserpicPlayer) {
		_videoUserpicPlayer = std::make_unique<Ui::VideoUserpicPlayer>([=] {
			if (!_gifPausedChecker()) {
				update();
			}
		});
	}
	_videoUserpicPlayer->setup(_peer, photo);
}

void TopBar::setupShowLastSeen(
		not_null<Window::SessionController*> controller) {
	const auto user = _peer->asUser();
	if (!user
		|| user->isSelf()
		|| user->isBot()
		|| user->isServiceUser()
		|| !user->session().premiumPossible()) {
		_showLastSeen->hide(anim::type::instant);
		return;
	}

	if (user->session().premium()) {
		if (user->lastseen().isHiddenByMe()) {
			user->updateFullForced();
		}
		_showLastSeen->hide(anim::type::instant);
		return;
	}

	rpl::combine(
		user->session().changes().peerFlagsValue(
			user,
			Data::PeerUpdate::Flag::OnlineStatus),
		Data::AmPremiumValue(&user->session())
	) | rpl::on_next([=](auto, bool premium) {
		const auto wasShown = _showLastSeen->toggled();
		const auto hiddenByMe = user->lastseen().isHiddenByMe();
		const auto shown = hiddenByMe
			&& !user->lastseen().isOnline(base::unixtime::now())
			&& !premium
			&& user->session().premiumPossible();
		_showLastSeen->toggle(shown, anim::type::instant);
		if (wasShown && premium && hiddenByMe) {
			user->updateFullForced();
		}
	}, _showLastSeen->lifetime());

	controller->session().api().userPrivacy().value(
		Api::UserPrivacy::Key::LastSeen
	) | rpl::filter([=](Api::UserPrivacy::Rule rule) {
		return (rule.option == Api::UserPrivacy::Option::Everyone);
	}) | rpl::on_next([=] {
		if (user->lastseen().isHiddenByMe()) {
			user->updateFullForced();
		}
	}, _showLastSeen->lifetime());

	_showLastSeen->setOpacity(0.);

	_showLastSeen->entity()->setFullRadius(true);

	_showLastSeen->entity()->setClickedCallback([=] {
		const auto type = Ui::ShowOrPremium::LastSeen;
		controller->show(Box(
			Ui::ShowOrPremiumBox,
			type,
			user->shortName(),
			[=] {
				controller->session().api().userPrivacy().save(
					::Api::UserPrivacy::Key::LastSeen,
					{});
			},
			[=] {
				::Settings::ShowPremium(controller, u"lastseen_hidden"_q);
			}));
	});
}

void TopBar::setupAnimatedPattern(const QRect &userpicGeometry) {
	_animatedPoints = GenerateAnimatedPattern(userpicGeometry.isNull()
		? TopBar::userpicGeometry()
		: userpicGeometry);
}

void TopBar::paintAnimatedPattern(
		QPainter &p,
		const QRect &clip,
		const QRect &userpicGeometry) {
	if (!_patternEmoji || !_patternEmoji->ready()) {
		return;
	}

	{
		// TODO make it better.
		if (_lastUserpicRect != userpicGeometry) {
			_lastUserpicRect = userpicGeometry;
			setupAnimatedPattern(userpicGeometry);
		}
	}

	if (_basePatternImage.isNull()) {
		auto patternColors = CalculatePatternColors(
			effectiveColorProfile(),
			effectiveCollectible(),
			_edgeColor.current(),
			Window::Theme::IsNightMode());
		const auto ratio = style::DevicePixelRatio();
		const auto scale = 0.910;
		const auto size = st::emojiSize;
		_basePatternImage = QImage(
			QSize(size, size) * ratio,
			QImage::Format_ARGB32_Premultiplied);
		_basePatternImage.setDevicePixelRatio(ratio);
		_basePatternImage.fill(Qt::transparent);
		auto painter = QPainter(&_basePatternImage);
		auto hq = PainterHighQualityEnabler(painter);
		// const auto contentSize = size * scale;
		// const auto offset = (size - contentSize) / 2.;
		// painter.translate(offset, offset);
		painter.scale(scale, scale);
		_patternEmoji->paint(painter, { .textColor = Qt::white });
		painter.resetTransform();

		if (patternColors.useOverlayBlend) {
			painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
			painter.fillRect(
				Rect(Size(size)),
				QColor(0, 0, 0, int(0.8 * 255)));
		} else {
			painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
			painter.fillRect(Rect(Size(size)), patternColors.patternColor);
		}
	}

	const auto progress = _progress.current();
	// const auto collapseDiff = progress >= 0.85 ? 1. : (progress / 0.85);
	// const auto collapse = std::clamp((collapseDiff - 0.2) / 0.8, 0., 1.);
	const auto collapse = progress;

	const auto userpicCenter = rect::center(userpicGeometry);
	const auto yOffset = 12 * (1. - progress);
	const auto imageSize = _basePatternImage.size()
		/ style::DevicePixelRatio();
	const auto halfImageWidth = imageSize.width() * 0.5;
	const auto halfImageHeight = imageSize.height() * 0.5;

	for (const auto &point : _animatedPoints) {
		const auto timeRange = point.endTime - point.startTime;
		const auto collapseProgress = (1. - collapse <= point.startTime)
			? 1.
			: 1.
				- std::clamp(
					(1. - collapse - point.startTime) / timeRange,
					0.,
					1.);

		if (collapseProgress <= 0.) {
			continue;
		}

		auto x = point.basePosition.x();
		auto y = point.basePosition.y() - yOffset;
		auto r = point.size * 0.5;

		if (collapseProgress < 1.) {
			const auto dx = x - userpicCenter.x();
			const auto dy = y - userpicCenter.y();
			x = userpicCenter.x() + dx * collapseProgress;
			y = userpicCenter.y() + dy * collapseProgress;
			r = kMinPatternRadius
				+ (r - kMinPatternRadius) * collapseProgress;
		}

		const auto scale = r / (point.size * 0.5);
		const auto scaledHalfWidth = halfImageWidth * scale;
		const auto scaledHalfHeight = halfImageHeight * scale;

		// Distance-based alpha calculation.
		const auto userpicRect = _lastUserpicRect;
		const auto acx = userpicRect.x() + userpicRect.width() / 2.;
		const auto acy = userpicRect.y() + userpicRect.height() / 2.;
		const auto aw = userpicRect.width();
		const auto ah = userpicRect.height();
		const auto distance = std::sqrt((x - acx) * (x - acx)
			+ (y - acy) * (y - acy));
		const auto normalizedDistance = std::clamp(
			distance / (aw * 2.),
			0.,
			1.);
		const auto distanceAlpha = 1. - normalizedDistance;

		// Bottom alpha calculation.
		const auto bottomThreshold = userpicRect.y() + ah + kMinPatternRadius;
		const auto bottomAlpha = (y > bottomThreshold)
			? 1. - std::clamp((y - bottomThreshold) / 56., 0., 1.)
			: 1.;

		auto alpha = progress * distanceAlpha * 0.5 * bottomAlpha;
		if (collapseProgress < 1.) {
			alpha = alpha * collapseProgress;
		}

		const auto target = QRectF(
			x - scaledHalfWidth,
			y - scaledHalfHeight,
			scaledHalfWidth * 2,
			scaledHalfHeight * 2);
		if (!target.intersects(QRectF(clip))) {
			continue;
		}

		p.setOpacity(alpha);
		p.drawImage(target, _basePatternImage);
	}
	p.setOpacity(1.);
}

void TopBar::setupPinnedToTopGifts(
		not_null<Window::SessionController*> controller) {
	const auto requestDone = crl::guard(this, [=](
			std::vector<Data::SavedStarGift> gifts) {
		const auto shouldHideFirst = _pinnedToTopGiftsFirstTimeShowed
			&& !_pinnedToTopGifts.empty();

		if (shouldHideFirst) {
			_giftsHiding = std::make_unique<Ui::Animations::Simple>();
			_giftsHiding->start([=](float64 value) {
				update();
				if (value <= 0.) {
					_giftsHiding = nullptr;
					_pinnedToTopGifts.clear();
					_giftsLoadingLifetime.destroy();
					updateCollectibleStatus();
					setupNewGifts(controller, gifts);
				}
			}, 1., 0., 300, anim::linear);
			return;
		}

		_pinnedToTopGifts.clear();
		_giftsLoadingLifetime.destroy();

		updateCollectibleStatus();
		setupNewGifts(controller, gifts);
	});
	_peer->session().recentSharedGifts().request(_peer, requestDone, true);
}

void TopBar::setupNewGifts(
		not_null<Window::SessionController*> controller,
		const std::vector<Data::SavedStarGift> &gifts) {
	const auto emojiStatusId = _peer->emojiStatusId().collectible
		? _peer->emojiStatusId().collectible->id
		: CollectibleId(0);
	auto filteredGifts = std::vector<Data::SavedStarGift>();
	const auto subtract = emojiStatusId ? 1 : 0;
	filteredGifts.reserve((gifts.size() > subtract)
		? (gifts.size() - subtract)
		: 0);
	for (const auto &gift : gifts) {
		if (const auto &unique = gift.info.unique) {
			if (unique->id != emojiStatusId) {
				filteredGifts.push_back(gift);
			}
		}
	}

	_pinnedToTopGifts.reserve(filteredGifts.size());
	if (filteredGifts.empty()) {
		_giftsAppearing = nullptr;
		_lottiePlayer = nullptr;
		_pinnedToTopGiftsFirstTimeShowed = true;
	} else if (!_lottiePlayer) {
		_lottiePlayer = std::make_unique<Lottie::MultiPlayer>(
			Lottie::Quality::Default);
		_lottiePlayer->updates() | rpl::on_next([=] {
			update();
		}, lifetime());
	}

	_giftsAppearing = std::make_unique<Ui::Animations::Simple>();

	constexpr auto kMaxPinnedToTopGifts = 6;

	auto positions = ranges::views::iota(
		0,
		kMaxPinnedToTopGifts) | ranges::to_vector;
	ranges::shuffle(positions);

	for (auto i = 0;
		i < filteredGifts.size() && i < kMaxPinnedToTopGifts;
		++i) {
		const auto &gift = filteredGifts[i];
		const auto document = _peer->owner().document(
			gift.info.document->id);
		auto entry = PinnedToTopGiftEntry();
		entry.manageId = gift.manageId;
		entry.media = document->createMediaView();
		entry.media->checkStickerSmall();
		if (const auto &unique = gift.info.unique) {
			if (unique->backdrop.centerColor.isValid()
				&& unique->backdrop.edgeColor.isValid()) {
				entry.centerColor = unique->backdrop.centerColor;
				entry.edgeColor = unique->backdrop.edgeColor;
				entry.bg = Ui::CreateTopBgGradient(
					Size(st::infoProfileTopBarGiftSize * 2),
					unique->backdrop.centerColor,
					anim::with_alpha(unique->backdrop.edgeColor, 0.0),
					false);
			}
		}
		if (!entry.centerColor.isValid()) {
			entry.centerColor = QColor(255, 206, 92);
			entry.edgeColor = QColor(186, 126, 36);
		}
		entry.orbitPhase = i * 1.0472 + 0.35;
		entry.orbitSpeed = 0.84 + (i % 3) * 0.16;
		entry.position = positions[i];
		entry.button = base::make_unique_q<Ui::AbstractButton>(this);
		entry.button->setAccessibleName([&] {
			const auto base = tr::lng_profile_action_short_gift(tr::now);

			if (const auto &unique = gift.info.unique) {
				const auto name = Data::UniqueGiftName(*unique);
				if (!name.isEmpty()) {
					return base + ": " + name;
				}
			}

			return base;
		}());
		entry.button->show();

		const auto index = int(_pinnedToTopGifts.size());
		entry.button->setClickedCallback([=, giftData = gift, peer = _peer] {
			::Settings::ShowSavedStarGiftBox(controller, peer, giftData);
		});
		base::install_event_filter(
			entry.button.get(),
			[=, giftData = gift, peer = _peer](not_null<QEvent*> e) {
				if (index < 0 || index >= int(_pinnedToTopGifts.size())) {
					return base::EventFilterResult::Continue;
				}
				auto &item = _pinnedToTopGifts[index];
				const auto bubblesOn = profileV2Enabled()
					&& Extera::Bool(u"profile_v2_gift_bubbles"_q)
					&& (_source != Source::Preview);
				switch (e->type()) {
				case QEvent::Enter:
					item.hovered = true;
					if (bubblesOn) {
						updateProfileV2AnimState();
						update();
					}
					return base::EventFilterResult::Continue;
				case QEvent::Leave:
					item.hovered = false;
					if (bubblesOn) {
						update();
					}
					return base::EventFilterResult::Continue;
				case QEvent::Hide:
				case QEvent::WindowDeactivate:
					item.physDragging = false;
					return base::EventFilterResult::Continue;
				case QEvent::MouseButtonPress: {
					if (!bubblesOn || _progress.current() <= 0.35) {
						return base::EventFilterResult::Continue;
					}
					const auto me = static_cast<QMouseEvent*>(e.get());
					if (me->button() != Qt::LeftButton) {
						return base::EventFilterResult::Continue;
					}
					if (!item.physInited) {
						item.physPos = calculateGiftPosition(
							item.position,
							_progress.current(),
							userpicGeometry());
						item.physInited = true;
					}
					const auto local = QPointF(mapFromGlobal(QCursor::pos()));
					item.physDragging = true;
					item.physMoved = false;
					item.dragStartLocal = local;
					item.dragGrabOffset = item.physPos - local;
					item.physVel = QPointF();
					item.lastDragTime = crl::now();
					updateProfileV2AnimState();
					me->accept();
					update();
					return base::EventFilterResult::Cancel;
				}
				case QEvent::MouseMove: {
					if (!item.physDragging) {
						return base::EventFilterResult::Continue;
					}
					if (!(QGuiApplication::mouseButtons() & Qt::LeftButton)) {
						item.physDragging = false;
						return base::EventFilterResult::Continue;
					}
					const auto me = static_cast<QMouseEvent*>(e.get());
					const auto local = QPointF(mapFromGlobal(QCursor::pos()));
					if (!item.physMoved) {
						const auto moved = std::hypot(
							local.x() - item.dragStartLocal.x(),
							local.y() - item.dragStartLocal.y());
						if (moved > style::ConvertFloatScale(5.0)) {
							item.physMoved = true;
						}
					}
					const auto r = GiftBubbleRadius();
					const auto pad = r + style::ConvertFloatScale(4.0);
					const auto maxX = std::max(pad + 1., float64(width()) - pad);
					const auto maxY = std::max(pad + 1., float64(height()) - pad);
					const auto target = QPointF(
						std::clamp(local.x() + item.dragGrabOffset.x(), pad, maxX),
						std::clamp(local.y() + item.dragGrabOffset.y(), pad, maxY));
					const auto now = crl::now();
					const auto dt = std::max(0.008, (now - item.lastDragTime) / 1000.);
					const auto rawVel = (target - item.physPos) / dt;
					item.physVel = item.physVel * 0.45 + rawVel * 0.55;
					const auto speed = std::hypot(item.physVel.x(), item.physVel.y());
					constexpr auto kMaxThrowSpeed = 1350.;
					if (speed > kMaxThrowSpeed) {
						item.physVel *= (kMaxThrowSpeed / speed);
					}
					item.physPos = target;
					item.lastDragTime = now;
					if (item.button) {
						const auto sz = int(std::ceil(r * 2.));
						const auto halfSz = sz / 2.;
						item.button->setGeometry(QRect(
							QPoint(
								int(std::round(target.x() - halfSz)),
								int(std::round(target.y() - halfSz))),
							Size(sz)));
					}
					me->accept();
					update();
					return base::EventFilterResult::Cancel;
				}
				case QEvent::MouseButtonRelease: {
					if (!item.physDragging) {
						return base::EventFilterResult::Continue;
					}
					const auto me = static_cast<QMouseEvent*>(e.get());
					const auto wasMoved = item.physMoved;
					item.physDragging = false;
					item.physMoved = false;
					me->accept();
					if (!wasMoved) {
						::Settings::ShowSavedStarGiftBox(controller, peer, giftData);
					} else {
						updateProfileV2AnimState();
					}
					update();
					return base::EventFilterResult::Cancel;
				}
				default:
					return base::EventFilterResult::Continue;
				}
			});

		_pinnedToTopGifts.push_back(std::move(entry));
	}
	if (_close) {
		_close->raise();
	}
	if (_topBarButton) {
		_topBarButton->raise();
	}
	if (_back) {
		_back->raise();
	}
	updateGiftButtonsGeometry(_progress.current(), userpicGeometry());
	updateProfileV2AnimState();

	using namespace ChatHelpers;

	rpl::single(
		rpl::empty_value()
	) | rpl::then(
		_peer->session().downloaderTaskFinished()
	) | rpl::on_next([=] {
		auto allLoaded = true;
		for (auto &entry : _pinnedToTopGifts) {
			if (!entry.animation && !entry.lastFrame.isNull()) {
				continue;
			}
			if (!entry.animation && entry.media->loaded()) {
				entry.animation = LottieAnimationFromDocument(
					_lottiePlayer.get(),
					entry.media.get(),
					StickerLottieSize::PinnedProfileUniqueGiftSize,
					Size(st::infoProfileTopBarGiftSize)
						* style::DevicePixelRatio());
			} else if (!entry.media->loaded()) {
				allLoaded = false;
			}
		}
		if (allLoaded) {
			_giftsLoadingLifetime.destroy();
			_giftsAppearing->stop();
			_giftsAppearing->start([=](float64 value) {
				update();
				if (value >= 1.) {
					_giftsAppearing = nullptr;
					_pinnedToTopGiftsFirstTimeShowed = true;
					if (_lottieSingleLoop) {
						auto allFramesCaptured = true;
						for (const auto &entry : _pinnedToTopGifts) {
							if (entry.animation || entry.lastFrame.isNull()) {
								allFramesCaptured = false;
								break;
							}
						}
						if (allFramesCaptured) {
							_lottiePlayer = nullptr;
						}
					}
				}
			}, 0., 1., 400, anim::easeOutQuint);
		}
	}, _giftsLoadingLifetime);
}

QPointF TopBar::calculateGiftPosition(
		int position,
		float64 progress,
		const QRect &userpicRect) const {
	const auto acx = userpicRect.x() + userpicRect.width() / 2.;
	const auto acy = userpicRect.y() + userpicRect.height() / 2.;
	const auto aw = userpicRect.width();
	const auto ah = userpicRect.height();

	auto giftPos = QPointF();
	auto delayValue = 0.;
	switch (position) {
	case 0: // Left.
		giftPos = QPointF(
			acx / 2. - st::infoProfileTopBarGiftLeft.x(),
			acy - st::infoProfileTopBarGiftLeft.y());
		delayValue = 1.6;
		break;
	case 1: // Top left.
		giftPos = QPointF(
			acx * 2. / 3. - st::infoProfileTopBarGiftTopLeft.x(),
			userpicRect.y() - st::infoProfileTopBarGiftTopLeft.y());
		delayValue = 0.;
		break;
	case 2: // Bottom left.
		giftPos = QPointF(
			acx * 2. / 3. - st::infoProfileTopBarGiftBottomLeft.x(),
			userpicRect.y() + ah - st::infoProfileTopBarGiftBottomLeft.y());
		delayValue = 0.9;
		break;
	case 3: // Right.
		giftPos = QPointF(
			acx + aw / 2. + st::infoProfileTopBarGiftRight.x(),
			acy - st::infoProfileTopBarGiftRight.y());
		delayValue = 1.6;
		break;
	case 4: // Top right.
		giftPos = QPointF(
			acx + aw / 3. + st::infoProfileTopBarGiftTopRight.x(),
			userpicRect.y() - st::infoProfileTopBarGiftTopRight.y());
		delayValue = 0.9;
		break;
	default: // Bottom right.
		giftPos = QPointF(
			acx + aw / 3. + st::infoProfileTopBarGiftBottomRight.x(),
			userpicRect.y() + ah - st::infoProfileTopBarGiftBottomRight.y());
		delayValue = 0.;
		break;
	}

	const auto delayFraction = 0.2;
	const auto maxDelayFraction = 1.6 * delayFraction;
	const auto intervalFraction = 1. - maxDelayFraction;
	const auto delay = delayValue * delayFraction;
	const auto collapse = (progress >= 1. - delay)
		? 1.
		: std::clamp((progress - maxDelayFraction + delay)
			/ intervalFraction, 0., 1.);

	if (collapse < 1.) {
		const auto collapseX = 1. - std::pow(1. - collapse, 2.);
		giftPos = QPointF(
			acx + (giftPos.x() - acx) * collapseX,
			acy + (giftPos.y() - acy) * collapse);
	}

	return giftPos;
}

void TopBar::paintPinnedToTopGifts(
		QPainter &p,
		const QRect &clip,
		const QRect &userpicRect) {
	if (_pinnedToTopGifts.empty() || _source == Source::Preview) {
		return;
	}

	const auto progress = _giftsHiding
		? _progress.current() * _giftsHiding->value(1.)
		: (_giftsAppearing
			? _progress.current() * _giftsAppearing->value(0.)
			: _progress.current());
	if (progress <= 0.) {
		return;
	}

	const auto v2Bubbles = profileV2Enabled()
		&& Extera::Bool(u"profile_v2_gift_bubbles"_q);

	for (auto &gift : _pinnedToTopGifts) {
		if (v2Bubbles) {
			if (gift.physDragging) {
				continue;
			}
			paintSingleGiftBubble(p, gift, progress, clip, userpicRect);
			continue;
		}
		if (!gift.animation
			&& (_lottieSingleLoop ? gift.lastFrame.isNull() : true)) {
			continue;
		}

		const auto giftPos = calculateGiftPosition(
			gift.position,
			progress,
			userpicRect);

		const auto alpha = progress;
		if (alpha <= 0.) {
			continue;
		}

		p.setOpacity(alpha);
		auto frameToRender = QImage();
		if (_lottieSingleLoop && !gift.lastFrame.isNull()) {
			frameToRender = gift.lastFrame;
		} else if (gift.animation && gift.animation->ready()) {
			frameToRender = gift.animation->frame();
			frameToRender.setDevicePixelRatio(style::DevicePixelRatio());
			if (_lottiePlayer) {
				_lottiePlayer->markFrameShown();
			}
			if (_lottieSingleLoop && gift.animation->framesCount() > 0) {
				const auto currentFrame = gift.animation->frameIndex();
				const auto totalFrames = gift.animation->framesCount();
				if (currentFrame >= totalFrames - 1) {
					gift.lastFrame = frameToRender;
					gift.animation = nullptr;

					auto allDone = true;
					for (const auto &entry : _pinnedToTopGifts) {
						if (entry.animation) {
							allDone = false;
							break;
						}
					}
					if (allDone) {
						_lottiePlayer = nullptr;
					}
				}
			}
		}
		if (!frameToRender.isNull()) {
			const auto frameSize = frameToRender.width()
				/ style::DevicePixelRatio();
			const auto halfFrameSize = frameSize / 2.;
			const auto resultPos = QPointF(
				giftPos.x() - halfFrameSize,
				giftPos.y() - halfFrameSize);
			auto target = QRectF(resultPos, QSizeF(frameSize, frameSize));
			auto bgPos = QPointF();
			const auto bgSize = gift.bg.isNull()
				? 0
				: (gift.bg.width() / style::DevicePixelRatio());
			if (bgSize > 0) {
				bgPos = QPointF(
					resultPos.x() + (frameSize - bgSize) / 2.,
					resultPos.y() + (frameSize - bgSize) / 2.);
				target = target.united(
					QRectF(bgPos, QSizeF(bgSize, bgSize)));
			}
			if (target.intersects(QRectF(clip))) {
				if (bgSize > 0) {
					p.drawImage(bgPos, gift.bg);
				}
				p.drawImage(resultPos, frameToRender);
			}
		}
	}
	p.setOpacity(1.);
}

void TopBar::paintSingleGiftBubble(
		QPainter &p,
		PinnedToTopGiftEntry &gift,
		float64 progress,
		const QRect &clip,
		const QRect &userpicRect) {
	if (!gift.animation
		&& (_lottieSingleLoop ? gift.lastFrame.isNull() : true)) {
		return;
	}

	auto frameToRender = QImage();
	if (_lottieSingleLoop && !gift.lastFrame.isNull()) {
		frameToRender = gift.lastFrame;
	} else if (gift.animation && gift.animation->ready()) {
		frameToRender = gift.animation->frame();
		frameToRender.setDevicePixelRatio(style::DevicePixelRatio());
		if (_lottiePlayer) {
			_lottiePlayer->markFrameShown();
		}
		if (_lottieSingleLoop && gift.animation->framesCount() > 0) {
			const auto currentFrame = gift.animation->frameIndex();
			const auto totalFrames = gift.animation->framesCount();
			if (currentFrame >= totalFrames - 1) {
				gift.lastFrame = frameToRender;
				gift.animation = nullptr;

				auto allDone = true;
				for (const auto &entry : _pinnedToTopGifts) {
					if (entry.animation) {
						allDone = false;
						break;
					}
				}
				if (allDone) {
					_lottiePlayer = nullptr;
				}
			}
		}
	}
	if (frameToRender.isNull()) {
		return;
	}

	const auto homePos = calculateGiftPosition(
		gift.position,
		progress,
		userpicRect);
	auto pos = homePos;
	if (gift.physInited) {
		if (progress >= 0.99 || gift.physDragging) {
			pos = gift.physPos;
		} else {
			pos = QPointF(
				homePos.x() + (gift.physPos.x() - homePos.x()) * progress,
				homePos.y() + (gift.physPos.y() - homePos.y()) * progress);
		}
	}

	const auto r = GiftBubbleRadius();
	const auto haloR = r * (gift.physDragging ? 1.88 : 1.60);
	const auto musicScale = (Extera::Bool(u"profile_v2_music_sync"_q)
		&& _v2MusicEnergy > 0.005)
		? (1. + _v2MusicEnergy * 0.065)
		: 1.;
	const auto scale = std::clamp(progress, 0., 1.)
		* gift.hoverScale
		* (gift.physDragging ? 1.12 : 1.)
		* musicScale;
	const auto boundR = haloR * scale * std::max(gift.stretchX, gift.stretchY) + 4.;
	const auto bounds = QRectF(
		pos.x() - boundR,
		pos.y() - boundR,
		boundR * 2.,
		boundR * 2.);
	if (!bounds.intersects(QRectF(clip))) {
		return;
	}

	const auto glassStyle = Extera::Int(u"profile_v2_glass_style"_q);
	const auto cCenter = gift.centerColor.isValid()
		? gift.centerColor
		: QColor(255, 206, 92);
	const auto cEdge = gift.edgeColor.isValid()
		? gift.edgeColor
		: cCenter.darker(145);

	p.save();
	auto hq = PainterHighQualityEnabler(p);
	p.setOpacity(std::clamp(progress, 0., 1.));
	p.translate(pos);
	if (std::abs(gift.stretchX - 1.) > 0.003
		|| std::abs(gift.stretchY - 1.) > 0.003) {
		const auto deg = gift.stretchAngle * 180. / M_PI;
		p.rotate(deg);
		p.scale(gift.stretchX * scale, gift.stretchY * scale);
		p.rotate(-deg);
	} else if (std::abs(scale - 1.) > 0.002) {
		p.scale(scale, scale);
	}

	// 1. Soft colored ambient glow halo behind the glass orb.
	{
		auto halo = QRadialGradient(QPointF(0., 0.), haloR);
		const auto peakAlpha = (glassStyle == 2)
			? 0.50
			: (gift.physDragging ? 0.44 : 0.34);
		halo.setColorAt(0., anim::with_alpha(cCenter, peakAlpha));
		halo.setColorAt(0.54, anim::with_alpha(cCenter, peakAlpha * 0.40));
		halo.setColorAt(1., anim::with_alpha(cEdge, 0.));
		p.setPen(Qt::NoPen);
		p.setBrush(halo);
		p.drawEllipse(QRectF(-haloR, -haloR, haloR * 2., haloR * 2.));
	}

	const auto sphereRect = QRectF(-r, -r, r * 2., r * 2.);
	auto clipPath = QPainterPath();
	clipPath.addEllipse(sphereRect);

	// 2. 3D glass sphere body fill & internal caustics.
	p.save();
	p.setClipPath(clipPath);
	p.setPen(Qt::NoPen);
	if (glassStyle == 0) {
		// Crystal Glass (iOS): refractive core + upper-left gloss + lower-right bounce.
		auto core = QRadialGradient(
			QPointF(-r * 0.24, -r * 0.28),
			r * 1.38);
		core.setColorAt(0., anim::with_alpha(cCenter.lighter(142), 0.46));
		core.setColorAt(0.56, anim::with_alpha(cCenter, 0.30));
		core.setColorAt(1., anim::with_alpha(cEdge, 0.56));
		p.setBrush(core);
		p.drawEllipse(sphereRect);

		auto topGloss = QRadialGradient(
			QPointF(-r * 0.36, -r * 0.44),
			r * 0.88);
		topGloss.setColorAt(0., QColor(255, 255, 255, 160));
		topGloss.setColorAt(0.46, QColor(255, 255, 255, 40));
		topGloss.setColorAt(1., QColor(255, 255, 255, 0));
		p.setBrush(topGloss);
		p.drawEllipse(sphereRect);

		auto bottomBounce = QRadialGradient(
			QPointF(r * 0.38, r * 0.42),
			r * 0.72);
		bottomBounce.setColorAt(0., anim::with_alpha(cCenter.lighter(168), 0.44));
		bottomBounce.setColorAt(1., anim::with_alpha(cCenter, 0.));
		p.setBrush(bottomBounce);
		p.drawEllipse(sphereRect);
	} else if (glassStyle == 1) {
		// Frosted Matte: rich translucent satin disc with delicate top sheen.
		auto matte = QLinearGradient(0., -r, 0., r);
		matte.setColorAt(0., anim::with_alpha(cCenter.lighter(120), 0.56));
		matte.setColorAt(1., anim::with_alpha(cEdge, 0.66));
		p.setBrush(matte);
		p.drawEllipse(sphereRect);

		auto frost = QLinearGradient(0., -r, 0., 0.);
		frost.setColorAt(0., QColor(255, 255, 255, 58));
		frost.setColorAt(1., QColor(255, 255, 255, 0));
		p.setBrush(frost);
		p.drawEllipse(sphereRect);
	} else {
		// Soft Glow: light translucent glass capsule.
		p.setBrush(anim::with_alpha(cCenter, 0.24));
		p.drawEllipse(sphereRect);
	}

	// 3. Crisp animated gift sticker centered inside the sphere.
	const auto iconSize = r * 1.34;
	p.drawImage(
		QRectF(-iconSize * 0.5, -iconSize * 0.5, iconSize, iconSize),
		frameToRender);

	// 4. Subtle top-left glass specular highlight catchlight.
	if (glassStyle == 0) {
		p.save();
		p.translate(-r * 0.34, -r * 0.46);
		p.rotate(-28.);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(255, 255, 255, 88));
		p.drawEllipse(QRectF(-r * 0.28, -r * 0.13, r * 0.56, r * 0.26));
		p.restore();
	}
	p.restore();

	// 5. Directional Liquid-Glass specular rim stroke.
	{
		auto rim = QLinearGradient(-r, -r, r, r);
		const auto topAlpha = (glassStyle == 0) ? 228 : (glassStyle == 1 ? 155 : 112);
		const auto midAlpha = (glassStyle == 0) ? 50 : 32;
		const auto botAlpha = (glassStyle == 0) ? 0.72 : 0.44;
		rim.setColorAt(0.00, QColor(255, 255, 255, topAlpha));
		rim.setColorAt(0.34, QColor(255, 255, 255, midAlpha));
		rim.setColorAt(0.68, QColor(255, 255, 255, 18));
		rim.setColorAt(1.00, anim::with_alpha(cCenter.lighter(155), botAlpha));
		const auto penW = style::ConvertFloatScale(glassStyle == 0 ? 1.35 : 1.05);
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(QBrush(rim), penW));
		const auto inset = penW * 0.5;
		p.drawEllipse(sphereRect.adjusted(inset, inset, -inset, -inset));
	}

	p.restore();
}

void TopBar::setupStoryOutline(const QRect &geometry) {
	const auto user = _peer->asUser();
	const auto channel = _peer->asChannel();
	if (!user && !channel) {
		return;
	}

	rpl::combine(
		_edgeColor.value(),
		rpl::merge(
			rpl::single(rpl::empty_value()),
			style::PaletteChanged(),
			_peer->session().changes().peerUpdates(
				Data::PeerUpdate::Flag::StoriesState
					| Data::PeerUpdate::Flag::ColorProfile
			) | rpl::filter([=](const Data::PeerUpdate &update) {
				return update.peer == _peer;
			}) | rpl::to_empty,
			_peer->owner().stories().sourceChanged(
			) | rpl::filter([=](PeerId peerId) {
				return peerId == _peer->id;
			}) | rpl::to_empty)
	) | rpl::on_next([=](
			std::optional<QColor> edgeColor,
			rpl::empty_value) {
		const auto geometry = QRectF(userpicGeometry());
		const auto colorProfile
			= _peer->session().api().peerColors().colorProfileFor(_peer);
		const auto hasProfileColor = colorProfile
			&& colorProfile->story.size() > 1;
		if (hasProfileColor) {
			edgeColor = std::nullopt;
		}
		_storyOutlineBrush = hasProfileColor
			? Ui::UnreadStoryOutlineGradient(
				geometry,
				colorProfile->story[0],
				colorProfile->story[1])
			: Ui::UnreadStoryOutlineGradient(geometry);
		updateStoryOutline(edgeColor);
	}, lifetime());
}

void TopBar::updateStoryOutline(std::optional<QColor> edgeColor) {
	const auto user = _peer->asUser();
	const auto channel = _peer->asChannel();
	if ((!user && !channel) || _savedMessages) {
		return;
	}

	const auto hasActiveStories = (_source == Source::Preview)
		? true
		: (user ? user->hasActiveStories() : channel->hasActiveStories());
	const auto hasLiveStories = (_source == Source::Preview)
		? false
		: (user ? user->hasActiveVideoStream() : false);

	if (_hasStories != hasActiveStories
		|| _hasLiveStories != hasLiveStories) {
		_hasStories = hasActiveStories;
		_hasLiveStories = hasLiveStories;
		update();
	}

	if (!hasActiveStories) {
		_storySegments.clear();
		return;
	}
	const auto widthBig = style::ConvertFloatScale(3.0);

	_storySegments.clear();

	if (_source == Source::Preview) {
		const auto colorProfile = effectiveColorProfile();
		const auto hasProfileColor = colorProfile
			&& colorProfile->story.size() > 1;
		const auto previewBrush = hasProfileColor
			? Ui::UnreadStoryOutlineGradient(
				QRectF(userpicGeometry()),
				colorProfile->story[0],
				colorProfile->story[1])
			: _localCollectible
			? Ui::UnreadStoryOutlineGradient(
				QRectF(userpicGeometry()),
				Ui::BlendColors(_localCollectible->edgeColor, Qt::white, .5),
				Ui::BlendColors(_localCollectible->edgeColor, Qt::white, .5))
			: Ui::UnreadStoryOutlineGradient(QRectF(userpicGeometry()));
		_storySegments.push_back({
			.brush = QBrush(previewBrush),
			.width = widthBig,
		});
		return;
	}

	auto &stories = _peer->owner().stories();
	const auto source = stories.source(_peer->id);
	if (!source) {
		stories.requestPeerStories(_peer);
		return;
	}

	const auto baseColor = edgeColor
		? Ui::BlendColors(*edgeColor, Qt::white, .5)
		: _storyOutlineBrush.color();
	const auto unreadBrush = _hasLiveStories
		? st::attentionButtonFg->b
		: edgeColor
		? QBrush(baseColor)
		: _storyOutlineBrush;
	const auto readBrush = edgeColor
		? QBrush(anim::with_alpha(baseColor, 0.5))
		: QBrush(st::dialogsUnreadBgMuted->b);

	if (_hasLiveStories) {
		_storySegments.push_back({
			.brush = unreadBrush,
			.width = widthBig,
		});
	} else {
		const auto readTill = source->readTill;
		const auto widthSmall = widthBig / 2.;
		for (const auto &storyIdDates : source->ids) {
			const auto isUnread = (storyIdDates.id > readTill);
			_storySegments.push_back({
				.brush = isUnread ? unreadBrush : readBrush,
				.width = !isUnread ? widthSmall : widthBig,
			});
		}
	}
}

void TopBar::paintStoryOutline(QPainter &p, const QRect &geometry) {
	if (!_hasStories || _storySegments.empty()) {
		return;
	}
	auto hq = PainterHighQualityEnabler(p);

	const auto progress = _progress.current();
	const auto alpha = std::clamp(
		(progress - kStoryOutlineFadeEnd) / kStoryOutlineFadeRange,
		0.,
		1.);
	if (alpha <= 0.) {
		return;
	}

	p.setOpacity(alpha);
	const auto outlineWidth = style::ConvertFloatScale(4.0);
	const auto padding = style::ConvertFloatScale(3.0);
	const auto outlineRect = QRectF(geometry).adjusted(
		-padding - outlineWidth / 2,
		-padding - outlineWidth / 2,
		padding + outlineWidth / 2,
		padding + outlineWidth / 2);

	if (_peer->isForum() || _source == Source::Community) {
		const auto radius = geometry.width()
			* Ui::ForumUserpicRadiusMultiplier()
			+ padding
			+ outlineWidth / 2;
		Ui::PaintOutlineSegments(p, outlineRect, radius, _storySegments);
	} else {
		Ui::PaintOutlineSegments(p, outlineRect, _storySegments);
	}

	if (_hasLiveStories) {
		const auto outline = _edgeColor.current().value_or(
			_solidBg.value_or(st::boxDividerBg->c));
		Ui::PaintLiveBadge(
			p,
			geometry.x(),
			geometry.y() + outlineWidth + padding,
			geometry.width(),
			outline);
	}
}

void TopBar::bindStatus() {
	if (_customStatus) {
		rpl::duplicate(
			_customStatus
		) | rpl::on_next([=](TextWithEntities text) {
			_status->setMarkedText(std::move(text));
			updateStatusPosition(_progress.current());
		}, _status->lifetime());
		_status->widthValue() | rpl::on_next([=] {
			updateStatusPosition(_progress.current());
		}, _status->lifetime());
		return;
	}
	_peer->session().changes().peerFlagsValue(
		_peer,
		Data::PeerUpdate::Flag::OnlineStatus | Data::PeerUpdate::Flag::Members
	) | rpl::on_next([=] {
		_statusLabel->refresh();
	}, lifetime());
}

void TopBar::setupStatusWithRating() {
	_status->setAttribute(Qt::WA_TransparentForMouseEvents);
	if (const auto rating = _starsRating.get()) {
		_statusShift = rating->widthValue();
		_statusShift.changes() | rpl::on_next([=] {
			updateLabelsPosition();
		}, _status->lifetime());
		rating->raise();
	}
}

const style::FlatLabel &TopBar::statusStyle() const {
	return _peer->isMegagroup()
		? st::infoProfileMegagroupCover.status
		: st::infoProfileCover.status;
}

rpl::producer<QString> TopBar::nameValue() const {
	if (_savedMessages) {
		return tr::lng_saved_messages();
	} else if (const auto topic = _key.topic()) {
		return Info::Profile::TitleValue(topic);
	}
	return Info::Profile::NameValue(_peer);
}

TopBarActionButtonStyle TopBar::mapActionStyle(
		std::optional<QColor> c) const {
	if (c) {
		return TopBarActionButtonStyle{
			.bgColor = Ui::BlendColors(
				*c,
				Qt::black,
				st::infoProfileTopBarActionButtonBgOpacity),
			.fgColor = ContentColorOverBackground(*c),
			.shadowColor = std::nullopt,
		};
	} else {
		return TopBarActionButtonStyle{
			.bgColor = anim::with_alpha(
				st::boxBg->c,
				1. - st::infoProfileTopBarActionButtonBgOpacity),
			.fgColor = std::nullopt,
			.shadowColor = Window::Theme::IsNightMode()
				? std::nullopt
				: std::make_optional(st::windowShadowFgFallback->c),
		};
	}
}

bool TopBar::profileV2Enabled() const {
	return Extera::Bool(u"profile_v2_enabled"_q);
}

bool TopBar::useAdaptiveAvatarBg() const {
	return profileV2Enabled()
		&& Extera::Bool(u"profile_v2_adaptive_avatar"_q)
		&& (_source != Source::Preview)
		&& !_savedMessages
		&& !_topic
		&& !effectiveCollectible()
		&& !effectiveColorProfile();
}

void TopBar::refreshProfileV2State() {
	if (!profileV2Enabled() || !Extera::Bool(u"profile_v2_gift_bubbles"_q)) {
		for (auto &gift : _pinnedToTopGifts) {
			gift.physDragging = false;
			gift.physMoved = false;
			gift.physInited = false;
			gift.physVel = QPointF();
			gift.stretchX = 1.;
			gift.stretchY = 1.;
			gift.hoverScale = 1.;
		}
	}
	if (useAdaptiveAvatarBg() && !_cachedUserpic.isNull()
		&& (!_adaptiveCenterColor.isValid() || _adaptiveAvatarKey != _userpicUniqueKey)) {
		extractAdaptiveAvatarColors();
	} else {
		applyProfileV2AdaptiveColors();
	}
	updateGiftButtonsGeometry(_progress.current(), userpicGeometry());
	updateProfileV2AnimState();
	if (_actionsShadow) {
		_actionsShadow->update();
	}
	update();
}

void TopBar::extractAdaptiveAvatarColors() {
	if (_cachedUserpic.isNull() || _savedMessages || _topic) {
		return;
	}
	_adaptiveAvatarKey = _userpicUniqueKey;
	const auto extracted = ExtractAvatarProfileColors(
		_cachedUserpic,
		Window::Theme::IsNightMode());
	_adaptiveCenterColor = extracted.center;
	_adaptiveEdgeColor = extracted.edge;
	if (!useAdaptiveAvatarBg() || _v2AdaptivePending) {
		return;
	}
	_v2AdaptivePending = true;
	crl::on_main(this, [=] {
		_v2AdaptivePending = false;
		applyProfileV2AdaptiveColors();
	});
}

void TopBar::applyProfileV2AdaptiveColors() {
	const auto collectible = effectiveCollectible();
	const auto colorProfile = effectiveColorProfile();
	const auto useAdaptive = useAdaptiveAvatarBg()
		&& _adaptiveCenterColor.isValid()
		&& _adaptiveEdgeColor.isValid();
	_hasGradientBg = (collectible != nullptr)
		|| (colorProfile && colorProfile->bg.size() > 1)
		|| useAdaptive;
	_solidBg = (!useAdaptive && colorProfile && colorProfile->bg.size() == 1)
		? std::make_optional(colorProfile->bg.front())
		: std::nullopt;
	_cachedGradient = QImage();
	_cachedClipPath = QPainterPath();
	adjustColors(collectible
		? std::optional<QColor>(collectible->edgeColor)
		: (colorProfile && !colorProfile->bg.empty())
		? std::optional<QColor>(colorProfile->bg.front())
		: useAdaptive
		? std::optional<QColor>(_adaptiveEdgeColor)
		: std::nullopt);
	updateButtonsColorOverride();
	update();
}

void TopBar::updateProfileV2AnimState() {
	if (!_v2AnimTimer) {
		return;
	}
	const auto v2 = profileV2Enabled()
		&& (_source != Source::Preview)
		&& !_savedMessages;
	const auto bubblesActive = v2
		&& Extera::Bool(u"profile_v2_gift_bubbles"_q)
		&& !_pinnedToTopGifts.empty();
	const auto musicActive = v2
		&& Extera::Bool(u"profile_v2_music_sync"_q)
		&& (_v2MusicPlaying || _v2MusicEnergy > 0.005);
	const auto shouldRun = isVisible()
		&& !anim::Disabled()
		&& (_progress.current() > 0.12)
		&& (bubblesActive || musicActive);
	if (shouldRun) {
		if (!_v2AnimTimer->isActive()) {
			_v2LastTick = crl::now();
			_v2AnimTimer->callEach(16);
		}
	} else {
		if (_v2AnimTimer->isActive()) {
			_v2AnimTimer->cancel();
		}
		if (!musicActive) {
			_v2MusicEnergy = 0.;
		}
	}
}

void TopBar::stepProfileV2Physics() {
	if (!isVisible() || !profileV2Enabled() || anim::Disabled()) {
		updateProfileV2AnimState();
		return;
	}
	const auto progress = _progress.current();
	if (progress <= 0.12) {
		updateProfileV2AnimState();
		return;
	}

	const auto now = crl::now();
	const auto dt = std::clamp((now - _v2LastTick) / 1000., 0.004, 0.045);
	_v2LastTick = now;
	_v2Elapsed += dt;

	// Update music rhythm pulse envelope.
	const auto musicSync = Extera::Bool(u"profile_v2_music_sync"_q);
	if (musicSync) {
		_v2MusicPlaying = Media::Player::ShowPauseIcon(
			Media::Player::instance()->getState(AudioMsgId::Type::Song).state);
	} else {
		_v2MusicPlaying = false;
	}
	if (_v2MusicPlaying) {
		_v2MusicPhase += dt * 2. * M_PI * 2.15;
		const auto s1 = 0.5 + 0.5 * std::sin(_v2MusicPhase);
		const auto s2 = 0.5 + 0.5 * std::sin(_v2MusicPhase * 2. + 0.65);
		const auto beat = std::pow(s1, 4.) * 0.72 + std::pow(s2, 3.) * 0.28;
		_v2MusicEnergy += (beat - _v2MusicEnergy) * (1. - std::exp(-16. * dt));
	} else if (_v2MusicEnergy > 0.001) {
		_v2MusicEnergy *= std::exp(-9. * dt);
		if (_v2MusicEnergy < 0.003) {
			_v2MusicEnergy = 0.;
		}
	}

	// Step 2D liquid gift bubble physics.
	const auto userpicRect = userpicGeometry();
	const auto avatarCenter = QRectF(userpicRect).center();
	const auto avatarR = userpicRect.width() * 0.5;
	const auto r = GiftBubbleRadius();
	const auto pad = r + style::ConvertFloatScale(4.0);
	const auto maxX = std::max(pad + 1., float64(width()) - pad);
	const auto maxY = std::max(pad + 1., float64(height()) - pad);

	if (Extera::Bool(u"profile_v2_gift_bubbles"_q)
		&& !_pinnedToTopGifts.empty()
		&& width() > 0) {
		for (auto &gift : _pinnedToTopGifts) {
			const auto baseHome = calculateGiftPosition(
				gift.position,
				1.,
				userpicRect);
			if (!gift.physInited) {
				gift.physPos = baseHome;
				gift.physVel = QPointF();
				gift.physInited = true;
			}
			const auto driftAmp = style::ConvertFloatScale(3.6);
			const auto t = _v2Elapsed * gift.orbitSpeed + gift.orbitPhase;
			auto home = QPointF(
				baseHome.x() + std::sin(t * 1.15) * driftAmp,
				baseHome.y() + std::cos(t * 0.92 + 0.7) * driftAmp);
			if (_v2MusicEnergy > 0.005 && musicSync) {
				const auto dir = baseHome - avatarCenter;
				const auto len = std::hypot(dir.x(), dir.y());
				if (len > 1.) {
					const auto boost = _v2MusicEnergy
						* style::ConvertFloatScale(5.5);
					home += (dir / len) * boost;
				}
			}

			if (!gift.physDragging) {
				constexpr auto kSpring = 2.8;
				gift.physVel += (home - gift.physPos) * (kSpring * dt);
				gift.physVel *= std::exp(-1.95 * dt);
				gift.physPos += gift.physVel * dt;

				// Elastic TopBar wall bounce.
				constexpr auto kWallBounce = 0.78;
				if (gift.physPos.x() < pad) {
					gift.physPos.setX(pad);
					if (gift.physVel.x() < 0.) {
						gift.physVel.setX(-gift.physVel.x() * kWallBounce);
					}
				} else if (gift.physPos.x() > maxX) {
					gift.physPos.setX(maxX);
					if (gift.physVel.x() > 0.) {
						gift.physVel.setX(-gift.physVel.x() * kWallBounce);
					}
				}
				if (gift.physPos.y() < pad) {
					gift.physPos.setY(pad);
					if (gift.physVel.y() < 0.) {
						gift.physVel.setY(-gift.physVel.y() * kWallBounce);
					}
				} else if (gift.physPos.y() > maxY) {
					gift.physPos.setY(maxY);
					if (gift.physVel.y() > 0.) {
						gift.physVel.setY(-gift.physVel.y() * kWallBounce);
					}
				}

				// Elastic circular avatar collision.
				const auto minAvatarDist = avatarR
					+ r
					+ style::ConvertFloatScale(2.5);
				const auto delta = gift.physPos - avatarCenter;
				const auto dist = std::hypot(delta.x(), delta.y());
				if (dist < minAvatarDist) {
					const auto nx = (dist > 0.001) ? (delta.x() / dist) : 1.;
					const auto ny = (dist > 0.001) ? (delta.y() / dist) : 0.;
					gift.physPos = QPointF(
						avatarCenter.x() + nx * minAvatarDist,
						avatarCenter.y() + ny * minAvatarDist);
					const auto vn = gift.physVel.x() * nx
						+ gift.physVel.y() * ny;
					if (vn < 0.) {
						constexpr auto kAvatarBounce = 0.80;
						gift.physVel -= QPointF(nx, ny)
							* ((1. + kAvatarBounce) * vn);
					}
				}
			}
		}

		// Elastic inter-bubble marble collisions.
		const auto minPairDist = r * 2. + style::ConvertFloatScale(1.5);
		const auto count = int(_pinnedToTopGifts.size());
		for (auto i = 0; i < count; ++i) {
			for (auto j = i + 1; j < count; ++j) {
				auto &a = _pinnedToTopGifts[i];
				auto &b = _pinnedToTopGifts[j];
				const auto d = b.physPos - a.physPos;
				const auto dist = std::hypot(d.x(), d.y());
				if (dist >= minPairDist || dist < 0.001) {
					continue;
				}
				const auto nx = d.x() / dist;
				const auto ny = d.y() / dist;
				const auto overlap = minPairDist - dist;
				if (a.physDragging && !b.physDragging) {
					b.physPos += QPointF(nx * overlap, ny * overlap);
					const auto rvn = (b.physVel.x() - a.physVel.x()) * nx
						+ (b.physVel.y() - a.physVel.y()) * ny;
					if (rvn < 0.) {
						b.physVel -= QPointF(nx, ny) * (1.82 * rvn);
					}
				} else if (b.physDragging && !a.physDragging) {
					a.physPos -= QPointF(nx * overlap, ny * overlap);
					const auto rvn = (b.physVel.x() - a.physVel.x()) * nx
						+ (b.physVel.y() - a.physVel.y()) * ny;
					if (rvn < 0.) {
						a.physVel += QPointF(nx, ny) * (1.82 * rvn);
					}
				} else if (!a.physDragging && !b.physDragging) {
					a.physPos -= QPointF(nx * overlap * 0.5, ny * overlap * 0.5);
					b.physPos += QPointF(nx * overlap * 0.5, ny * overlap * 0.5);
					const auto rvn = (b.physVel.x() - a.physVel.x()) * nx
						+ (b.physVel.y() - a.physVel.y()) * ny;
					if (rvn < 0.) {
						const auto imp = 0.91 * rvn;
						a.physVel += QPointF(nx * imp, ny * imp);
						b.physVel -= QPointF(nx * imp, ny * imp);
					}
				}
			}
		}

		// Velocity-driven jelly stretch & button hit-target sync.
		const auto ease = 1. - std::exp(-18. * dt);
		const auto hoverEase = 1. - std::exp(-16. * dt);
		const auto sz = int(std::ceil(r * 2.));
		const auto halfSz = sz / 2.;
		for (auto &gift : _pinnedToTopGifts) {
			const auto speed = std::hypot(gift.physVel.x(), gift.physVel.y());
			const auto stretchAmt = std::clamp(speed / 920., 0., 0.28);
			const auto targetSX = 1. + stretchAmt;
			const auto targetSY = 1. / (1. + stretchAmt * 0.72);
			gift.stretchX += (targetSX - gift.stretchX) * ease;
			gift.stretchY += (targetSY - gift.stretchY) * ease;
			if (speed > 18.) {
				const auto desiredAngle = std::atan2(
					gift.physVel.y(),
					gift.physVel.x());
				auto diff = desiredAngle - gift.stretchAngle;
				while (diff > M_PI) {
					diff -= 2. * M_PI;
				}
				while (diff < -M_PI) {
					diff += 2. * M_PI;
				}
				gift.stretchAngle += diff * ease;
			}
			const auto targetHover = gift.hovered ? 1.11 : 1.;
			gift.hoverScale += (targetHover - gift.hoverScale) * hoverEase;

			if (gift.button && !gift.physDragging) {
				gift.button->setGeometry(QRect(
					QPoint(
						int(std::round(gift.physPos.x() - halfSz)),
						int(std::round(gift.physPos.y() - halfSz))),
					Size(sz)));
			}
		}
	}

	updateProfileV2AnimState();
	update();
}

void TopBar::paintProfileV2Backdrop(
		QPainter &p,
		const QRect &clip,
		const QRect &userpicRect) {
	const auto progress = _progress.current();
	if (progress <= 0.01 || width() <= 0 || height() <= 0) {
		return;
	}
	const auto glassStyle = Extera::Int(u"profile_v2_glass_style"_q);
	const auto collectible = effectiveCollectible();
	const auto colorProfile = effectiveColorProfile();
	const auto accent = collectible
		? collectible->centerColor
		: (colorProfile && colorProfile->bg.size() > 1)
		? colorProfile->bg[1]
		: (colorProfile && !colorProfile->bg.empty())
		? colorProfile->bg.front()
		: (_adaptiveCenterColor.isValid()
			? _adaptiveCenterColor
			: st::windowBgActive->c);

	p.save();
	if (clipTouchesRoundedCorners(clip)) {
		if (_cachedClipPath.isEmpty()) {
			const auto radius = st::boxRadius;
			_cachedClipPath.addRoundedRect(
				rect() + QMargins{ 0, 0, 0, radius + 1 },
				radius,
				radius);
		}
		p.setClipPath(_cachedClipPath);
	}
	auto hq = PainterHighQualityEnabler(p);
	p.setPen(Qt::NoPen);

	// Soft breathing radial aura centered behind the avatar.
	const auto center = QRectF(userpicRect).center();
	const auto auraR = std::max(width() * 0.56, userpicRect.width() * 2.15);
	const auto auraAlpha = progress
		* ((glassStyle == 0 ? 0.24 : (glassStyle == 1 ? 0.18 : 0.22))
			+ _v2MusicEnergy * 0.16);
	auto aura = QRadialGradient(center, auraR);
	aura.setColorAt(0., anim::with_alpha(accent.lighter(135), auraAlpha));
	aura.setColorAt(0.55, anim::with_alpha(accent, auraAlpha * 0.35));
	aura.setColorAt(1., anim::with_alpha(accent, 0.));
	p.setBrush(aura);
	p.fillRect(rect().intersected(clip), aura);

	// Subtle top-edge crystalline highlight sheen in Crystal Glass mode.
	if (glassStyle == 0) {
		const auto sheenH = float64(style::ConvertFloatScale(28.0));
		if (clip.top() <= int(std::ceil(sheenH))) {
			auto topSheen = QLinearGradient(0., 0., 0., sheenH);
			topSheen.setColorAt(0., QColor(255, 255, 255, int(26 * progress)));
			topSheen.setColorAt(1., QColor(255, 255, 255, 0));
			p.fillRect(QRectF(0., 0., width(), sheenH), topSheen);
		}
	}

	// Smooth bottom fade into profile body divider background when using adaptive backdrop.
	if (useAdaptiveAvatarBg()) {
		const auto fadeH = std::max(
			height() * 0.18,
			float64(style::ConvertFloatScale(28.0)));
		const auto fadeTop = height() - fadeH;
		if (clip.bottom() >= int(std::floor(fadeTop))) {
			auto bottomFade = QLinearGradient(0., fadeTop, 0., height());
			const auto bodyBg = st::boxDividerBg->c;
			bottomFade.setColorAt(0., anim::with_alpha(bodyBg, 0.));
			bottomFade.setColorAt(1., anim::with_alpha(bodyBg, 0.52 * progress));
			p.fillRect(QRectF(0., fadeTop, width(), fadeH), bottomFade);
		}
	}

	p.restore();
}

void TopBar::paintProfileV2AvatarRim(
		QPainter &p,
		const QRect &userpicRect) {
	const auto rimMode = Extera::Int(u"profile_v2_avatar_rim"_q);
	if (rimMode == 2) {
		return;
	}
	if (_peer->isForum() || _source == Source::Community || _topicIconView) {
		return;
	}
	const auto progress = _progress.current();
	if (progress <= 0.15) {
		return;
	}
	const auto rimColorMode = Extera::Int(u"profile_v2_avatar_rim_color"_q);
	const auto rRect = QRectF(userpicRect);
	const auto ac = rRect.center();
	const auto ar = rRect.width() * 0.5;
	if (ar <= 2.) {
		return;
	}

	p.save();
	auto hq = PainterHighQualityEnabler(p);
	p.setBrush(Qt::NoBrush);

	// Base subtle crystal-glass rim edge around the avatar lens.
	{
		auto baseRim = QLinearGradient(rRect.topLeft(), rRect.bottomRight());
		baseRim.setColorAt(0., QColor(255, 255, 255, int(85 * progress)));
		baseRim.setColorAt(0.5, QColor(255, 255, 255, int(22 * progress)));
		baseRim.setColorAt(1., QColor(255, 255, 255, int(52 * progress)));
		const auto baseW = style::ConvertFloatScale(1.1);
		p.setPen(QPen(QBrush(baseRim), baseW));
		p.drawEllipse(rRect.adjusted(0.5, 0.5, -0.5, -0.5));
	}

	const auto bubbleR = GiftBubbleRadius();
	const auto resolveColor = [&](const QColor &source) {
		if (rimColorMode == 2) {
			return QColor(255, 255, 255);
		}
		if (rimColorMode == 1) {
			return MakeVividRimColor(source);
		}
		return source.isValid() ? source.lighter(132) : QColor(255, 215, 110);
	};

	if (rimMode == 0 && !_pinnedToTopGifts.empty()) {
		// Near Gift (iOS): per-gift directional colored light wrapping onto the avatar rim.
		for (const auto &gift : _pinnedToTopGifts) {
			const auto gPos = gift.physInited
				? gift.physPos
				: calculateGiftPosition(gift.position, progress, userpicRect);
			const auto dx = gPos.x() - ac.x();
			const auto dy = gPos.y() - ac.y();
			const auto dist = std::hypot(dx, dy);
			if (dist < 1.) {
				continue;
			}
			const auto edgeDist = std::max(0., dist - ar - bubbleR);
			auto prox = std::clamp(1. - edgeDist / (ar * 1.65), 0.16, 1.);
			if (gift.physDragging) {
				prox = std::min(1., prox * 1.35 + 0.18);
			}
			if (Extera::Bool(u"profile_v2_music_sync"_q)) {
				prox = std::min(1., prox + _v2MusicEnergy * 0.25);
			}

			// QConicalGradient angles increase counter-clockwise from +X (screen Y is down).
			auto deg = -std::atan2(dy, dx) * 180. / M_PI;
			while (deg < 0.) {
				deg += 360.;
			}
			while (deg >= 360.) {
				deg -= 360.;
			}
			const auto glowColor = resolveColor(gift.centerColor);

			auto cone = QConicalGradient(ac, deg);
			cone.setColorAt(0.00, anim::with_alpha(glowColor, 0.92 * prox));
			cone.setColorAt(0.11, anim::with_alpha(glowColor, 0.42 * prox));
			cone.setColorAt(0.24, anim::with_alpha(glowColor, 0.));
			cone.setColorAt(0.76, anim::with_alpha(glowColor, 0.));
			cone.setColorAt(0.89, anim::with_alpha(glowColor, 0.42 * prox));
			cone.setColorAt(1.00, anim::with_alpha(glowColor, 0.92 * prox));

			// Soft outer bloom (skip wide outer bloom if story outline owns outer ring).
			if (!_hasStories) {
				p.setOpacity(progress * 0.36);
				const auto bloomW = style::ConvertFloatScale(5.2);
				p.setPen(QPen(QBrush(cone), bloomW));
				const auto grow = bloomW * 0.28;
				p.drawEllipse(rRect.adjusted(-grow, -grow, grow, grow));
			}

			// Crisp specular edge highlight hugging the avatar perimeter.
			p.setOpacity(progress * 0.94);
			const auto rimW = style::ConvertFloatScale(1.85);
			p.setPen(QPen(QBrush(cone), rimW));
			p.drawEllipse(rRect.adjusted(0.4, 0.4, -0.4, -0.4));
		}
	} else if (rimMode == 1) {
		// Always: continuous luminous halo around avatar.
		const auto collectible = effectiveCollectible();
		const auto colorProfile = effectiveColorProfile();
		const auto fallback = collectible
			? collectible->centerColor
			: (colorProfile && !colorProfile->bg.empty())
			? colorProfile->bg.back()
			: (_adaptiveCenterColor.isValid()
				? _adaptiveCenterColor
				: st::windowBgActive->c);
		const auto glowColor = resolveColor(fallback);
		const auto pulse = 0.72 + _v2MusicEnergy * 0.28;
		if (!_hasStories) {
			p.setOpacity(progress * 0.30 * pulse);
			const auto bloomW = style::ConvertFloatScale(5.0);
			p.setPen(QPen(glowColor, bloomW));
			const auto grow = bloomW * 0.25;
			p.drawEllipse(rRect.adjusted(-grow, -grow, grow, grow));
		}
		p.setOpacity(progress * 0.85 * pulse);
		const auto rimW = style::ConvertFloatScale(1.75);
		p.setPen(QPen(glowColor, rimW));
		p.drawEllipse(rRect.adjusted(0.4, 0.4, -0.4, -0.4));
	}

	p.restore();
}

} // namespace Info::Profile
