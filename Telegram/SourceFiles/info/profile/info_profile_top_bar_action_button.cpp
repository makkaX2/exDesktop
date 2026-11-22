/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_top_bar_action_button.h"

#include "extera/extera_config.h"
#include "ui/effects/animation_value.h"
#include "ui/effects/ripple_animation.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "lottie/lottie_icon.h"
#include "styles/style_layers.h"
#include "styles/style_info.h"

#include <QtGui/QPainterPath>

namespace Info::Profile {
namespace {

constexpr auto kIconFadeStart = 0.4;
constexpr auto kIconFadeRange = 1.0 - kIconFadeStart;

[[nodiscard]] bool UseProfileV2CircularActions() {
	return Extera::Bool(u"profile_v2_enabled"_q)
		&& Extera::Bool(u"profile_v2_circular_actions"_q);
}

[[nodiscard]] QRect CircularOrbRect(int width) {
	const auto orbSize = std::max(
		std::min(width - style::ConvertScale(4), style::ConvertScale(34)),
		1);
	const auto orbLeft = (width - orbSize) / 2;
	const auto orbTop = style::ConvertScale(1);
	return QRect(orbLeft, orbTop, orbSize, orbSize);
}

} // namespace

TopBarActionButton::TopBarActionButton(
	not_null<QWidget*> parent,
	const QString &text,
	const QString &lottieName)
: RippleButton(parent, st::universalRippleAnimation)
, _text(text) {
	setupLottie(lottieName);
	Extera::Config::Instance().changes(
	) | rpl::filter([](const QString &key) {
		return key.startsWith(u"profile_v2_"_q);
	}) | rpl::on_next([=] {
		update();
	}, lifetime());
}

TopBarActionButton::TopBarActionButton(
	not_null<QWidget*> parent,
	const QString &text,
	const style::icon &icon)
: RippleButton(parent, st::universalRippleAnimation)
, _text(text)
, _icon(&icon) {
	Extera::Config::Instance().changes(
	) | rpl::filter([](const QString &key) {
		return key.startsWith(u"profile_v2_"_q);
	}) | rpl::on_next([=] {
		update();
	}, lifetime());
}

TopBarActionButton::~TopBarActionButton() = default;

void TopBarActionButton::setupLottie(const QString &lottieName) {
	_lottie = std::make_unique<Lottie::Icon>(Lottie::IconDescriptor{
		.name = lottieName,
		.color = _lottieColor,
		.sizeOverride = Size(st::infoProfileTopBarActionButtonLottieSize),
	});
	_lottie->animate([=] { update(); }, 0, _lottie->framesCount() - 1);
}

void TopBarActionButton::convertToToggle(
		const style::icon &offIcon,
		const style::icon &onIcon,
		const QString &offLottie,
		const QString &onLottie) {
	_isToggle = true;
	_offIcon = &offIcon;
	_onIcon = &onIcon;
	_offLottie = offLottie;
	_onLottie = onLottie;
	_icon = _offIcon;
	_lottie.reset();
}

void TopBarActionButton::toggle(bool state) {
	if (!_isToggle || _toggleState == state) {
		return;
	}
	_toggleState = state;
	const auto &lottie = _toggleState ? _onLottie : _offLottie;
	setupLottie(lottie);
	_lottie->animate([=] {
		update();
		if (_lottie->frameIndex() == _lottie->framesCount() - 1) {
			_icon = _toggleState ? _onIcon : _offIcon;
			_lottie.reset();
		}
	}, 0, _lottie->framesCount() - 1);
}

void TopBarActionButton::finishAnimating() {
	if (_lottie) {
		_icon = _toggleState ? _onIcon : _offIcon;
		_lottie.reset();
		update();
	}
}

void TopBarActionButton::setText(const QString &text) {
	_text = text;
	update();
}

void TopBarActionButton::setLottieColor(const style::color *color) {
	_lottieColor = color;
	_lottie.reset();
	update();
}

void TopBarActionButton::setStyle(const TopBarActionButtonStyle &style) {
	_bgColor = style.bgColor;
	_fgColor = style.fgColor;
	_rippleColor = _fgColor
		? std::make_optional(
			anim::with_alpha(
				*_fgColor,
				st::infoProfileTopBarBackdropRippleOpacity))
		: std::nullopt;
	update();
}

void TopBarActionButton::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);

	const auto progress = float64(height())
		/ st::infoProfileTopBarActionButtonSize;
	p.setOpacity(progress);

	const auto v2 = Extera::Bool(u"profile_v2_enabled"_q);
	const auto circular = UseProfileV2CircularActions();
	const auto glassStyle = Extera::Int(u"profile_v2_glass_style"_q);
	const auto labelText = (v2 && Extera::Bool(u"profile_v2_lowercase_actions"_q))
		? _text.toLower()
		: _text;
	const auto hovered = _overAnimation.value(isOver() ? 1. : 0.);

	if (circular) {
		const auto orb = CircularOrbRect(width());
		const auto orbF = QRectF(orb);
		const auto orbScale = (progress > 0.2)
			? std::clamp((progress - 0.2) / 0.8, 0., 1.)
			: 0.;
		if (orbScale > 0.) {
			const auto swell = 1. + hovered * 0.045;
			const auto center = orbF.center();
			p.save();
			auto hq = PainterHighQualityEnabler(p);
			p.translate(center);
			p.scale(orbScale * swell, orbScale * swell);
			p.translate(-center);

			// Translucent Liquid-Glass circular orb fill.
			p.setPen(Qt::NoPen);
			if (glassStyle == 0) {
				auto fillGrad = QLinearGradient(
					orbF.topLeft(),
					orbF.bottomLeft());
				fillGrad.setColorAt(0., _bgColor.lighter(118));
				fillGrad.setColorAt(1., _bgColor);
				p.setBrush(fillGrad);
			} else {
				p.setBrush(_bgColor);
			}
			p.drawEllipse(orbF);

			// Upper-left specular gloss dome in Crystal Glass mode.
			if (glassStyle == 0) {
				auto glossPath = QPainterPath();
				glossPath.addEllipse(orbF);
				p.save();
				p.setClipPath(glossPath);
				auto gloss = QRadialGradient(
					QPointF(
						orbF.left() + orbF.width() * 0.30,
						orbF.top() + orbF.height() * 0.20),
					orbF.width() * 0.72);
				gloss.setColorAt(0., QColor(255, 255, 255, 72));
				gloss.setColorAt(1., QColor(255, 255, 255, 0));
				p.setBrush(gloss);
				p.drawEllipse(orbF);
				p.restore();
			}

			if (hovered > 0.) {
				const auto hoverColor = _rippleColor
					? *_rippleColor
					: QColor(255, 255, 255, 34);
				p.setOpacity(progress * hovered);
				p.setBrush(hoverColor);
				p.drawEllipse(orbF);
				p.setOpacity(progress);
			}

			p.restore();

			paintRipple(
				p,
				orb.x(),
				orb.y(),
				_rippleColor ? &*_rippleColor : nullptr);

			// Directional specular glass rim stroke.
			p.save();
			auto hqRim = PainterHighQualityEnabler(p);
			p.translate(center);
			p.scale(orbScale * swell, orbScale * swell);
			p.translate(-center);
			auto rim = QLinearGradient(orbF.topLeft(), orbF.bottomRight());
			const auto hiAlpha = (glassStyle == 0)
				? 172
				: (glassStyle == 1 ? 105 : 72);
			const auto loAlpha = (glassStyle == 0)
				? 96
				: (glassStyle == 1 ? 56 : 36);
			rim.setColorAt(0.00, QColor(255, 255, 255, hiAlpha));
			rim.setColorAt(0.36, QColor(255, 255, 255, 34));
			rim.setColorAt(0.70, QColor(255, 255, 255, 16));
			rim.setColorAt(1.00, QColor(255, 255, 255, loAlpha));
			const auto penW = style::ConvertFloatScale(
				glassStyle == 0 ? 1.2 : 1.0);
			const auto inset = penW * 0.5;
			p.setBrush(Qt::NoBrush);
			p.setPen(QPen(QBrush(rim), penW));
			p.drawEllipse(orbF.adjusted(inset, inset, -inset, -inset));
			p.restore();
		}

		const auto iconSize = st::infoProfileTopBarActionButtonIconSize;
		const auto iconTop = orbF.top() + (orbF.height() - iconSize) / 2.;
		if (_lottie || _icon) {
			const auto iconScale = (progress > kIconFadeStart)
				? (progress - kIconFadeStart) / kIconFadeRange
				: 0.0;
			p.setOpacity(iconScale);
			p.save();
			const auto iconLeft = (width() - iconSize) / 2.;
			const auto half = iconSize / 2.;
			const auto iconCenter = QPointF(iconLeft + half, iconTop + half);
			p.translate(iconCenter);
			p.scale(iconScale, iconScale);
			p.translate(-iconCenter);
			p.translate(iconLeft, iconTop);
			if (_lottie) {
				_lottie->paint(p, 0, 0, _fgColor);
			} else if (_icon) {
				if (_fgColor) {
					_icon->paint(p, 0, 0, width(), *_fgColor);
				} else {
					_icon->paint(p, 0, 0, width());
				}
			}
			p.restore();
			p.setOpacity(progress);
		}

		const auto textTop = orb.y() + orb.height() + style::ConvertScale(2);
		p.setClipRect(rect());
		if (_fgColor.has_value()) {
			p.setPen(*_fgColor);
		} else {
			p.setPen(st::windowBoldFg);
		}
		p.setFont(st::infoProfileTopBarActionButtonFont);
		const auto textScale = std::max(kIconFadeStart, progress);
		const auto textRect = rect() - QMargins(0, textTop, 0, 0);
		const auto textCenter = rect::center(textRect);
		p.translate(textCenter);
		p.scale(textScale, textScale);
		p.translate(-textCenter);
		const auto elidedText = st::infoProfileTopBarActionButtonFont->elided(
			labelText,
			textRect.width(),
			Qt::ElideMiddle);
		p.drawText(textRect, elidedText, style::al_top);
		return;
	}

	p.setPen(Qt::NoPen);
	p.setBrush(_bgColor);
	{
		auto hq = PainterHighQualityEnabler(p);
		p.drawRoundedRect(rect(), st::infoProfileTopBarActionButtonRadius, st::infoProfileTopBarActionButtonRadius);

		if (v2 && glassStyle == 0) {
			auto sheen = QLinearGradient(0., 0., 0., height() * 0.55);
			sheen.setColorAt(0., QColor(255, 255, 255, 36));
			sheen.setColorAt(1., QColor(255, 255, 255, 0));
			p.setBrush(sheen);
			p.drawRoundedRect(rect(), st::infoProfileTopBarActionButtonRadius, st::infoProfileTopBarActionButtonRadius);
		}

		const auto hoverFill = _rippleColor
			? hovered
			: 0.;
		if (hoverFill > 0.) {
			p.setOpacity(progress * hoverFill);
			p.setBrush(*_rippleColor);
			p.drawRoundedRect(rect(), st::infoProfileTopBarActionButtonRadius, st::infoProfileTopBarActionButtonRadius);
			p.setOpacity(progress);
		}
	}

	paintRipple(p, 0, 0, _rippleColor ? &*_rippleColor : nullptr);

	if (v2) {
		auto hqRim = PainterHighQualityEnabler(p);
		const auto rF = QRectF(rect());
		const auto radius = float64(st::infoProfileTopBarActionButtonRadius);
		auto rim = QLinearGradient(rF.topLeft(), rF.bottomRight());
		const auto hiAlpha = (glassStyle == 0) ? 140 : (glassStyle == 1 ? 86 : 56);
		rim.setColorAt(0., QColor(255, 255, 255, hiAlpha));
		rim.setColorAt(0.42, QColor(255, 255, 255, 26));
		rim.setColorAt(1., QColor(255, 255, 255, hiAlpha / 2));
		const auto penW = style::ConvertFloatScale(1.1);
		const auto inset = penW * 0.5;
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(QBrush(rim), penW));
		p.drawRoundedRect(
			rF.adjusted(inset, inset, -inset, -inset),
			radius - inset,
			radius - inset);
	}

	const auto iconSize = st::infoProfileTopBarActionButtonIconSize;
	const auto iconTop = st::infoProfileTopBarActionButtonIconTop;

	if (_lottie || _icon) {
		const auto iconScale = (progress > kIconFadeStart)
			? (progress - kIconFadeStart) / kIconFadeRange
			: 0.0;
		p.setOpacity(iconScale);
		p.save();
		const auto iconLeft = (width() - iconSize) / 2.;
		const auto half = iconSize / 2.;
		const auto iconCenter = QPointF(iconLeft + half, iconTop + half);
		p.translate(iconCenter);
		p.scale(iconScale, iconScale);
		p.translate(-iconCenter);
		p.translate(iconLeft, iconTop);
		if (_lottie) {
			_lottie->paint(p, 0, 0, _fgColor);
		} else if (_icon) {
			if (_fgColor) {
				_icon->paint(p, 0, 0, width(), *_fgColor);
			} else {
				_icon->paint(p, 0, 0, width());
			}
		}
		p.restore();
		p.setOpacity(progress);
	}

	const auto skip = st::infoProfileTopBarActionButtonTextSkip;

	p.setClipRect(0, 0, width(), height() - skip);

	if (_fgColor.has_value()) {
		p.setPen(*_fgColor);
	} else {
		p.setPen(st::windowBoldFg);
	}

	p.setFont(st::infoProfileTopBarActionButtonFont);

	const auto textScale = std::max(kIconFadeStart, progress);
	const auto textRect = rect()
		- QMargins(0, st::infoProfileTopBarActionButtonTextTop, 0, 0);
	const auto textCenter = rect::center(textRect);
	p.translate(textCenter);
	p.scale(textScale, textScale);
	p.translate(-textCenter);

	const auto elidedText = st::infoProfileTopBarActionButtonFont->elided(
		labelText,
		textRect.width(),
		Qt::ElideMiddle);
	p.drawText(textRect, elidedText, style::al_top);
}

void TopBarActionButton::onStateChanged(
		State was,
		StateChangeSource source) {
	RippleButton::onStateChanged(was, source);

	const auto over = isOver();
	if (over != ((was & StateFlag::Over) != 0)) {
		_overAnimation.start(
			[=] { update(); },
			over ? 0. : 1.,
			over ? 1. : 0.,
			st::universalDuration);
	}
}

QImage TopBarActionButton::prepareRippleMask() const {
	if (UseProfileV2CircularActions()) {
		return Ui::RippleAnimation::EllipseMask(
			CircularOrbRect(width()).size());
	}
	return Ui::RippleAnimation::RoundRectMask(size(), st::infoProfileTopBarActionButtonRadius);
}

QPoint TopBarActionButton::prepareRippleStartPosition() const {
	if (UseProfileV2CircularActions()) {
		return mapFromGlobal(QCursor::pos()) - CircularOrbRect(width()).topLeft();
	}
	return mapFromGlobal(QCursor::pos());
}

} // namespace Info::Profile
