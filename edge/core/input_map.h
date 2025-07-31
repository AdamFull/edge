#pragma once

namespace edge {
	enum class KeyAction {
		eUnknown,
		ePress,
		eRelease,
		eHold
	};

	enum class KeyboardKeyCode {
		eUnknown,
		eSpace,
		eApostrophe, /* ' */
		eComma, /* , */
		eMinus, /* - */
		ePeriod, /* . */
		eSlash, /* / */
		e0,
		e1,
		e2,
		e3,
		e4,
		e5,
		e6,
		e7,
		e8,
		e9,
		eSemicolon, /* ; */
		eEq, /* = */
		eA,
		eB,
		eC,
		eD,
		eE,
		eF,
		eG,
		eH,
		eI,
		eJ,
		eK,
		eL,
		eM,
		eN,
		eO,
		eP,
		eQ,
		eR,
		eS,
		eT,
		eU,
		eV,
		eW,
		eX,
		eY,
		eZ,
		eLeftBracket, /* [ */
		eBackslash, /* \ */
		eRightBracket, /* ] */
		eGraveAccent, /* ` */
		eWorld1, /* non-US #1 */
		eWorld2, /* non-US #2 */

		/* Function keys */
		eEsc,
		eEnter,
		eTab,
		eBackspace,
		eInsert,
		eDel,
		eRight,
		eLeft,
		eDown,
		eUp,
		ePageUp,
		ePageDown,
		eHome,
		eEnd,
		eCapsLock,
		eScrollLock,
		eNumLock,
		ePrintScreen,
		ePause,
		eF1,
		eF2,
		eF3,
		eF4,
		eF5,
		eF6,
		eF7,
		eF8,
		eF9,
		eF10,
		eF11,
		eF12,
		eF13,
		eF14,
		eF15,
		eF16,
		eF17,
		eF18,
		eF19,
		eF20,
		eF21,
		eF22,
		eF23,
		eF24,
		eF25,
		eKp0,
		eKp1,
		eKp2,
		eKp3,
		eKp4,
		eKp5,
		eKp6,
		eKp7,
		eKp8,
		eKp9,
		eKpDec,
		eKpDiv,
		eKpMul,
		eKpSub,
		eKpAdd,
		eKpEnter,
		eKpEq,
		eLeftShift,
		eLeftControl,
		eLeftAlt,
		eLeftSuper,
		eRightShift,
		eRightControl,
		eRightAlt,
		eRightSuper,
		eMenu,

		eCount
	};

	enum class MouseKeyCode {
		eUnknown,
		eButton1,
		eButton2,
		eButton3,
		eButton4,
		eButton5,
		eButton6,
		eButton7,
		eButton8,
		eMouseLeft = eButton1,
		eMouseRight = eButton2,
		eMouseMiddle = eButton3,

		eCount
	};

	enum class GamepadKeyCode {
		eUnknown,
		eButtonA,
		eButtonB,
		eButtonX,
		eButtonY,
		eButtonCross = eButtonA,
		eButtonCircle = eButtonB,
		eButtonSquare = eButtonX,
		eButtonTriangle = eButtonY,
		eButtonLeftBumper,
		eButtonRightBumper,
		eButtonBack,
		eButtonStart,
		eButtonGuide,
		eButtonLeftThumb,
		eButtonRightThumb,
		eButtonDPadUp,
		eButtonDPadRight,
		eButtonDPadDown,
		eButtonDPadLeft
	};

	enum class GamepadAxisCode {
		eUnknown,
		eLeftX,
		eLeftY,
		eRightX,
		eRightY,
		eLeftTrigger,
		eRightTrigger
	};
}