package org.scummvm.scummvm;

import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.content.Context;
//import android.util.Log;
import android.view.KeyEvent;
import android.view.KeyCharacterMap;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.GestureDetector;
import android.view.HapticFeedbackConstants;
import android.view.InputDevice;
//import android.view.inputmethod.InputMethodManager;

import androidx.annotation.NonNull;

import java.lang.ref.WeakReference;

public class ScummVMEventsBase implements
		android.view.View.OnKeyListener,
		android.view.View.OnTouchListener,
		android.view.GestureDetector.OnGestureListener,
		android.view.GestureDetector.OnDoubleTapListener {

	public static final int JE_SYS_KEY = 0;
	public static final int JE_KEY = 1;
	public static final int JE_DPAD = 2;
	public static final int JE_DOWN = 3;
	public static final int JE_SCROLL = 4;
	public static final int JE_TAP = 5;
	public static final int JE_DOUBLE_TAP = 6;
	public static final int JE_MULTI = 7;
	public static final int JE_BALL = 8;
	public static final int JE_LMB_DOWN = 9;
	public static final int JE_LMB_UP = 10;
	public static final int JE_RMB_DOWN = 11;
	public static final int JE_RMB_UP = 12;
	public static final int JE_MOUSE_MOVE = 13;
	public static final int JE_GAMEPAD = 14;
	public static final int JE_JOYSTICK = 15;
	public static final int JE_MMB_DOWN = 16;
	public static final int JE_MMB_UP = 17;
	public static final int JE_BMB_DOWN = 18;
	public static final int JE_BMB_UP = 19;
	public static final int JE_FMB_DOWN = 20;
	public static final int JE_FMB_UP = 21;
	public static final int JE_MOUSE_WHEEL_UP = 22;
	public static final int JE_MOUSE_WHEEL_DOWN = 23;
	public static final int JE_TV_REMOTE = 24;
	public static final int JE_QUIT = 0x1000;
	public static final int JE_MENU = 0x1001;

	public static final int JACTION_DOWN = 0;
	public static final int JACTION_MOVE = 1;
	public static final int JACTION_UP = 2;
	public static final int JACTION_CANCEL = 3;

	public static final int TOUCH_MODE_TOUCHPAD = 0;
	public static final int TOUCH_MODE_MOUSE = 1;
	public static final int TOUCH_MODE_GAMEPAD = 2;
	public static final int TOUCH_MODE_MAX = 3;

	public static final int JOYSTICK_AXIS_MAX = 32767; // matches the definition in common/events of "const int16 JOYAXIS_MAX = 32767;"
	public static final float JOYSTICK_AXIS_HAT_SCALE = 0.66f; // ie. 2/3 to be applied to JOYSTICK_AXIS_MAX

	final protected Context _context;
	final protected ScummVM _scummvm;
	final protected GestureDetector _gd;
	final protected int _longPressTimeout;
	final protected MouseHelper _mouseHelper;
	final protected MultitouchHelper _multitouchHelper;

	protected View _currentView;
	protected int _touchMode;

	protected boolean _doubleTapMode;

	// Custom handler code (to avoid mem leaks, see warning "This Handler Class Should Be Static Or Leaks Might Occur”) based on:
	// https://stackoverflow.com/a/27826094
	public static class ScummVMEventHandler extends Handler {

		private final WeakReference<ScummVMEventsBase> mListenerReference;

		public ScummVMEventHandler(ScummVMEventsBase listener) {
			super(Looper.getMainLooper());
			mListenerReference = new WeakReference<>(listener);
		}

		@Override
		public synchronized void handleMessage(@NonNull Message msg) {
			ScummVMEventsBase listener = mListenerReference.get();
			if(listener != null) {
				listener.handleEVHMessage(msg);
			}
		}

		public void clear() {
			this.removeCallbacksAndMessages(null);
		}
	}

	final private ScummVMEventHandler _handler = new ScummVMEventHandler(this);

//	/**
//	 * An example getter to provide it to some external class
//	 * or just use 'new MyHandler(this)' if you are using it internally.
//	 * If you only use it internally you might even want it as final member:
//	 * private final MyHandler mHandler = new MyHandler(this);
//	 */
//	public Handler ScummVMEventHandler() {
//		return new ScummVMEventHandler(this);
//	}

	public ScummVMEventsBase(Context context, ScummVM scummvm, MouseHelper mouseHelper) {
		_context = context;
		_scummvm = scummvm;
		// Careful, _mouseHelper can be null (if HoverListener is not available for the device API -- old devices, API < 9)
		_mouseHelper = mouseHelper;

		_multitouchHelper = new MultitouchHelper(_scummvm);

		_gd = new GestureDetector(context, this);
		_gd.setOnDoubleTapListener(this);
		_gd.setIsLongpressEnabled(false);

		_doubleTapMode = false;
		_longPressTimeout = ViewConfiguration.getLongPressTimeout();
	}

	final static int MSG_SMENU_LONG_PRESS = 1;
	final static int MSG_SBACK_LONG_PRESS = 2;
	final static int MSG_LONG_TOUCH_EVENT = 3;

	private void handleEVHMessage(final Message msg) {
		if (msg.what == MSG_SMENU_LONG_PRESS) {
			// this toggles the android keyboard (see showVirtualKeyboard() in ScummVMActivity.java)
			// when menu key is long-pressed
//			InputMethodManager imm = (InputMethodManager)
//				_context.getSystemService(Context.INPUT_METHOD_SERVICE);
//
//			if (imm != null)
//				imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, 0);
			((ScummVMActivity) _context).toggleScreenKeyboard();
		} else if (msg.what == MSG_SBACK_LONG_PRESS) {
			_scummvm.pushEvent(JE_SYS_KEY,
			                   KeyEvent.ACTION_DOWN,
			                   KeyEvent.KEYCODE_MENU,
			                   0,
			                   0,
			                   0,
			                   0);
			_scummvm.pushEvent(JE_SYS_KEY,
			                   KeyEvent.ACTION_UP,
			                   KeyEvent.KEYCODE_MENU,
			                   0,
			                   0,
			                   0,
			                   0);
		} else if (msg.what == MSG_LONG_TOUCH_EVENT) {
			if (!_multitouchHelper.isMultitouchMode() && getTouchMode() != TOUCH_MODE_GAMEPAD && !_doubleTapMode) {
				_currentView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
			}
		}
	}

	final public int getTouchMode() {
		return _touchMode;
	}

	final public void setTouchMode(int touchMode) {
		assert (touchMode >= 0) && (touchMode < TOUCH_MODE_MAX);

		if (_touchMode == touchMode) {
			return;
		}

		if (_touchMode == TOUCH_MODE_GAMEPAD) {
			// We were in gamepad mode and we leave it
			_scummvm.updateTouch(JACTION_CANCEL, 0, 0, 0);
		}
		int oldTouchMode = _touchMode;
		_touchMode = touchMode;
		_scummvm.setupTouchMode(oldTouchMode, _touchMode);
	}

	final public int nextTouchMode() {
		if (_touchMode == TOUCH_MODE_GAMEPAD) {
			// We leave gamepad mode
			_scummvm.updateTouch(JACTION_CANCEL, 0, 0, 0);
		}
		int oldTouchMode = _touchMode;
		_touchMode = (_touchMode + 1) % TOUCH_MODE_MAX;
		_scummvm.setupTouchMode(oldTouchMode, _touchMode);

		return _touchMode;
	}

	public void clearEventHandler() {
		_handler.clear();
		_multitouchHelper.clearEventHandler();
	}

	final public void sendQuitEvent() {
		_scummvm.pushEvent(JE_QUIT, 0, 0, 0, 0, 0, 0);
	}

	public boolean onTrackballEvent(MotionEvent e) {
		//Log.d(ScummVM.LOG_TAG, "SCUMMV-EVENTS-BASE - onTrackballEvent");
		_scummvm.pushEvent(JE_BALL, e.getAction(),
			(int)(e.getX() * e.getXPrecision() * 100),
			(int)(e.getY() * e.getYPrecision() * 100),
			0, 0, 0);
		return true;
	}

	public boolean onGenericMotionEvent(MotionEvent e) {
		// we don't manage the GenericMotionEvent
		return false;
	}

	// OnKeyListener
	@Override
	final public boolean onKey(View v, int keyCode, KeyEvent e) {

//		String actionStr = "";
//		switch (e.getAction()) {
//			case KeyEvent.ACTION_UP:
//				actionStr = "KeyEvent.ACTION_UP";
//				break;
//			case KeyEvent.ACTION_DOWN:
//				actionStr = "KeyEvent.ACTION_DOWN";
//				break;
//			default:
//				if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q && e.getAction() == KeyEvent.ACTION_MULTIPLE) {
//					actionStr = "KeyEvent.ACTION_MULTIPLE";
//				} else {
//					actionStr = e.toString();
//				}
//		}
//		Log.d(ScummVM.LOG_TAG, "SCUMMV-EVENTS-BASE - onKEY:::" + keyCode + " Action::" + actionStr); // Called

		_currentView = v;
		final int action = e.getAction();

		int eventUnicodeChar = e.getUnicodeChar();
		if (e.getDeviceId() != 0) {
			// getDeviceId: Gets the id for the device that this event came from.
			// An id of zero indicates that the event didn't come from a physical device and maps to the default keymap.
			// The other numbers are arbitrary and you shouldn't depend on the values.
			final KeyCharacterMap m = KeyCharacterMap.load(e.getDeviceId());
			eventUnicodeChar = m.get(e.getKeyCode(), e.getMetaState());
		}

		if (eventUnicodeChar == (int)EditableAccommodatingLatinIMETypeNullIssues.ONE_UNPROCESSED_CHARACTER.charAt(0)) {
			//We are ignoring this character, and we want everyone else to ignore it, too, so
			// we return true indicating that we have handled it (by ignoring it).
			return true;
		}

		if (keyCode == 238) {
			// this (undocumented) event is sent when ACTION_HOVER_ENTER or ACTION_HOVER_EXIT occurs
			return false;
		}

		if (keyCode == KeyEvent.KEYCODE_BACK) {
			if (_mouseHelper != null) {
				if (MouseHelper.isMouse(e)) {
					// right mouse button was just clicked which sends an extra back button press (which should be ignored)
					return true;
				}
			}

			if (ScummVMActivity.keyboardWithoutTextInputShown ) {
				if (action == KeyEvent.ACTION_DOWN) {
					return true;
				} else if (action == KeyEvent.ACTION_UP) {
					// Hide keyboard
					if (((ScummVMActivity) _context).isScreenKeyboardShown()) {
						((ScummVMActivity) _context).hideScreenKeyboard();
					}
					return true;
				}
			}
		}

		if (e.isSystem() || keyCode == KeyEvent.KEYCODE_MENU) {
			// no repeats for system keys
			if (e.getRepeatCount() > 0) {
				return false;
			}

			// Have to reimplement hold-down-menu-brings-up-softkeybd
			// ourselves, since we are otherwise hijacking the menu key :(
			// See com.android.internal.policy.impl.PhoneWindow.onKeyDownPanel()
			// for the usual Android implementation of this feature.
			//
			// We adopt a similar behavior for the Back system button, as well.
			if (keyCode == KeyEvent.KEYCODE_MENU || keyCode == KeyEvent.KEYCODE_BACK) {
				//
				// Upon pressing the system menu or system back key:
				// (The example below assumes that system Back key was pressed)
				// 1. keyHandler.hasMessages(MSG_SBACK_LONG_PRESS) = false, and thus: fired = true
				// 2. Action will be KeyEvent.ACTION_DOWN, so a delayed message "MSG_SBACK_LONG_PRESS" will be sent to keyHandler after _longPressTimeout time
				//    The "MSG_SBACK_LONG_PRESS" will be handled (and removed) in the keyHandler.
				//    For the Back button, the keyHandler should forward a ACTION_UP for MENU (the alternate func of Back key!) to native)
				//    But if the code enters this section before the "MSG_SBACK_LONG_PRESS" was handled in keyHandler (probably due to a ACTION_UP)
				//        then fired = false and the message is removed from keyHandler, meaning we should treat the button press as a SHORT key press
				final int typeOfLongPressMessage;
				if (keyCode == KeyEvent.KEYCODE_MENU) {
					typeOfLongPressMessage = MSG_SMENU_LONG_PRESS;
				} else { // back button
					typeOfLongPressMessage = MSG_SBACK_LONG_PRESS;
				}

				final boolean fired = !_handler.hasMessages(typeOfLongPressMessage);

				_handler.removeMessages(typeOfLongPressMessage);

				if (action == KeyEvent.ACTION_DOWN) {
					_handler.sendMessageDelayed(_handler.obtainMessage(typeOfLongPressMessage), _longPressTimeout);
					return true;
				} else if (action != KeyEvent.ACTION_UP) {
					return true;
				}

				if (fired) {
					return true;
				}

				// It's still necessary to send a key down event to the backend.
//				Log.d(ScummVM.LOG_TAG, "JE_SYS_KEY");
				_scummvm.pushEvent(JE_SYS_KEY,
				                   KeyEvent.ACTION_DOWN,
				                   keyCode,
				                   eventUnicodeChar & KeyCharacterMap.COMBINING_ACCENT_MASK,
				                   e.getMetaState(),
				                   e.getRepeatCount(),
				                   (int)(e.getEventTime() - e.getDownTime()));
			}
		}

		// The KeyEvent.ACTION_MULTIPLE constant was deprecated in API level 29 (Q).
		// No longer used by the input system.
		// getAction() value: multiple duplicate key events have occurred in a row, or a complex string is being delivered.
		//    If the key code is not KEYCODE_UNKNOWN then the getRepeatCount() method returns the number of times the given key code should be executed.
		//    Otherwise, if the key code is KEYCODE_UNKNOWN, then this is a sequence of characters as returned by getCharacters().
		//    sequence of characters
		// getCharacters() is also deprecated in API level 29
		//    For the special case of a ACTION_MULTIPLE event with key code of KEYCODE_UNKNOWN,
		//    this is a raw string of characters associated with the event. In all other cases it is null.
		// TODO What is the use case for this?
		//  Does it make sense to keep it with a Build.VERSION.SDK_INT < Build.VERSION_CODES.Q check?
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
			if (action == KeyEvent.ACTION_MULTIPLE
				&& keyCode == KeyEvent.KEYCODE_UNKNOWN) {
				final KeyCharacterMap m = KeyCharacterMap.load(e.getDeviceId());
				final KeyEvent[] es = m.getEvents(e.getCharacters().toCharArray());

				if (es == null) {
					return true;
				}

				for (KeyEvent s : es) {
					_scummvm.pushEvent(JE_KEY,
						s.getAction(),
						s.getKeyCode(),
						eventUnicodeChar & KeyCharacterMap.COMBINING_ACCENT_MASK,
						s.getMetaState(),
						s.getRepeatCount(),
						0);
				}
				return true;
			}
		}

		int type;
		switch (keyCode) {
		case KeyEvent.KEYCODE_VOLUME_DOWN:
		case KeyEvent.KEYCODE_VOLUME_UP:
			// We ignore these so that they can be handled by Android.
			return false;

//		case KeyEvent.KEYCODE_CHANNEL_UP:
//		case KeyEvent.KEYCODE_CHANNEL_DOWN:
		case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
		case KeyEvent.KEYCODE_MEDIA_REWIND:
		case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
			type = JE_TV_REMOTE;
			break;

		case KeyEvent.KEYCODE_DPAD_UP:
		case KeyEvent.KEYCODE_DPAD_DOWN:
		case KeyEvent.KEYCODE_DPAD_LEFT:
		case KeyEvent.KEYCODE_DPAD_RIGHT:
		case KeyEvent.KEYCODE_DPAD_CENTER:
			// NOTE 1 For now, we're handling DPAD keys as JE_GAMEPAD events, regardless the source InputDevice
			//        We delegate these keypresses to ScummVM's keymapper as JOYSTICK_BUTTON_DPAD presses.
			//        (JOYSTICK_BUTTON_DPAD_UP, JOYSTICK_BUTTON_DPAD_DOWN, JOYSTICK_BUTTON_DPAD_LEFT, JOYSTICK_BUTTON_DPAD_RIGHT and JOYSTICK_BUTTON_DPAD_CENTER)
			//        By default mapped to virtual mouse (VMOUSE).
			//        As virtual mouse, cursor may be too fast/hard to control, so it's recommended to set and use a VMOUSESLOW binding too,
			//         (Simultaneous button pressing may work on physical TV remote controls, but may not work on apps for remote controls)
			//        or adjust the Pointer Speed setting from the "Control" tab.
			// NOTE 2 Modern gamepads/ game controllers treat the "DPAD" cross buttons as HATs that produce movement events
			// and *not* DPAD_UP/DOWN/LEFT/RIGHT button press events. Hence, for those controllers these DPAD key events won't be triggered.
			// Those are handled in ScummVMEventsModern class within its onGenericMotionEvent() implementation.
			//
			// fall-through
		case KeyEvent.KEYCODE_BUTTON_A:
		case KeyEvent.KEYCODE_BUTTON_B:
		case KeyEvent.KEYCODE_BUTTON_C:
		case KeyEvent.KEYCODE_BUTTON_X:
		case KeyEvent.KEYCODE_BUTTON_Y:
		case KeyEvent.KEYCODE_BUTTON_Z:
		case KeyEvent.KEYCODE_BUTTON_L1:
		case KeyEvent.KEYCODE_BUTTON_R1:
		case KeyEvent.KEYCODE_BUTTON_L2:
		case KeyEvent.KEYCODE_BUTTON_R2:
		case KeyEvent.KEYCODE_BUTTON_THUMBL:
		case KeyEvent.KEYCODE_BUTTON_THUMBR:
		case KeyEvent.KEYCODE_BUTTON_START:
		case KeyEvent.KEYCODE_BUTTON_SELECT:
		case KeyEvent.KEYCODE_BUTTON_MODE:
			type = JE_GAMEPAD;
			break;

		case KeyEvent.KEYCODE_BUTTON_1:
		case KeyEvent.KEYCODE_BUTTON_2:
		case KeyEvent.KEYCODE_BUTTON_3:
		case KeyEvent.KEYCODE_BUTTON_4:
			// These are oddly detected with SOURCE_KEYBOARD for joystick so don't bother checking the e.getSource()
			// Tested on a Saitek ST200 USB Control Stick & Throttle
			type = JE_JOYSTICK;
			break;

		default:
			if (e.isSystem()) {
				type = JE_SYS_KEY;
			} else {
				type = JE_KEY;
			}
			break;
		}

		//_scummvm.displayMessageOnOSD("GetKey: " + keyCode + " unic=" + eventUnicodeChar+ " arg3= " + (eventUnicodeChar& KeyCharacterMap.COMBINING_ACCENT_MASK) + " meta: " + e.getMetaState());
		//_scummvm.displayMessageOnOSD("GetKey: " + keyCode + " type=" + type + " source=" + e.getSource() + " action= " + action + " arg5= " + e.getRepeatCount());
		//Log.d(ScummVM.LOG_TAG,"GetKey: " + keyCode + " unic=" + eventUnicodeChar+ " arg3= " + (eventUnicodeChar& KeyCharacterMap.COMBINING_ACCENT_MASK) + " meta: " + e.getMetaState());

		// look in events.cpp for how this is handled
		_scummvm.pushEvent(type,
		                   action,
		                   keyCode,
		                   eventUnicodeChar & KeyCharacterMap.COMBINING_ACCENT_MASK,
		                   e.getMetaState(),
		                   e.getRepeatCount(),
		                   (int)(e.getEventTime() - e.getDownTime()));

		return true;
	}


	/** Aux method to provide a description for a MotionEvent action
	 *  Given an action int, returns a string description
	 *  Use for debug purposes
	 * @param action the id of the action (as returned by getAction()
	 * @return the action description
	 */
	public static String motionEventActionToString(int action) {
		switch (action) {

			case MotionEvent.ACTION_DOWN: return "Down";
			case MotionEvent.ACTION_MOVE: return "Move";
			case MotionEvent.ACTION_POINTER_DOWN: return "Pointer Down";
			case MotionEvent.ACTION_UP: return "Up";
			case MotionEvent.ACTION_POINTER_UP: return "Pointer Up";
			case MotionEvent.ACTION_OUTSIDE: return "Outside";
			case MotionEvent.ACTION_CANCEL: return "Cancel";
//			case MotionEvent.ACTION_POINTER_2_DOWN: return "Pointer 2 Down"; // 261 - deprecated (but still fired for Android 9, Mi device)
//			case MotionEvent.ACTION_POINTER_2_UP: return "Pointer 2 Up"; // 262 - deprecated (but still fired for Android 9, Mi device)
//			case MotionEvent.ACTION_POINTER_3_DOWN: return "Pointer 3 Down"; // 517 - deprecated (but still fired for Android 9, Mi device)
//			case MotionEvent.ACTION_POINTER_3_UP: return "Pointer 3 Up"; // 518 - deprecated (but still fired for Android 9, Mi device)
			default:
				if ((action &  MotionEvent.ACTION_POINTER_DOWN) == MotionEvent.ACTION_POINTER_DOWN) {
					return "Pointer Down ***";
				} else if ((action &  MotionEvent.ACTION_POINTER_UP) == MotionEvent.ACTION_POINTER_UP) {
					return "Pointer Up ***";
				}
				return "Unknown:: " + action;
		}
	}

	// OnTouchListener
	@Override
	final public boolean onTouch(View v, final MotionEvent event) {

		// Note: In this article https://developer.android.com/training/gestures/multi
		//       it is recommended to use MotionEventCompat helper methods, instead of directly using MotionEvent getAction() etc.
		//       However, getActionMasked() and MotionEventCompat *are deprecated*, and now direct use of MotionEvent methods is recommended.
		//       https://developer.android.com/reference/androidx/core/view/MotionEventCompat

		// Note 2: Do not return intentionally false for the onTouch function because then getPointerCount() won't work as intended
		//         ie. it will always return 1,
		//         as noted here:
		//         https://stackoverflow.com/a/11709964

		_currentView = v;
		final int action = event.getAction();

		// Get the index of the pointer associated with the action.
//		int index = event.getActionIndex();
//		int xPos = (int)event.getX(index);
//		int yPos = (int)event.getY(index);

//		String prefixDBGMsg = "SPECIAL DBG action is " + motionEventActionToString(action) + " ";
//		if (event.getPointerCount() > 1) {
//			// The coordinates of the current screen contact, relative to
//			// the responding View or Activity.
//			Log.d(ScummVM.LOG_TAG,prefixDBGMsg + "Multitouch event (" + event.getPointerCount() + "):: x:" + xPos + " y: " + yPos);
//		} else {
//			// Single touch event
//			Log.d(ScummVM.LOG_TAG,prefixDBGMsg + "Single touch event:: x: " + xPos + " y: " + yPos);
//		}

		if (ScummVMActivity.keyboardWithoutTextInputShown
		    && ((ScummVMActivity) _context).isScreenKeyboardShown()
		    && ((ScummVMActivity) _context).getScreenKeyboard().getY() <= event.getY() ) {
			event.offsetLocation(-((ScummVMActivity) _context).getScreenKeyboard().getX(), -((ScummVMActivity) _context).getScreenKeyboard().getY());
			// TODO maybe call the onTouchEvent of something else here?
			((ScummVMActivity) _context).getScreenKeyboard().onTouchEvent(event);
			// correct the offset for continuing handling the event
			event.offsetLocation(((ScummVMActivity) _context).getScreenKeyboard().getX(), ((ScummVMActivity) _context).getScreenKeyboard().getY());
		}

		if (_mouseHelper != null) {
			boolean isMouse = MouseHelper.isMouse(event);
			if (isMouse) {
				// mouse button is pressed
				return _mouseHelper.onMouseEvent(event, false);
			}
		}

		if (_touchMode == TOUCH_MODE_GAMEPAD) {
			switch (event.getActionMasked()) {
				case MotionEvent.ACTION_DOWN:
				case MotionEvent.ACTION_POINTER_DOWN: {
					int idx = event.getActionIndex();
					_scummvm.updateTouch(JACTION_DOWN, event.getPointerId(idx), (int)event.getX(idx), (int)event.getY(idx));
					// fall through
				}
				case MotionEvent.ACTION_MOVE:
					for(int idx = 0; idx < event.getPointerCount(); idx++) {
						_scummvm.updateTouch(JACTION_MOVE, event.getPointerId(idx), (int)event.getX(idx), (int)event.getY(idx));
					}
					break;
				case MotionEvent.ACTION_UP:
				case MotionEvent.ACTION_POINTER_UP: {
					int idx = event.getActionIndex();
					_scummvm.updateTouch(JACTION_UP, event.getPointerId(idx), (int)event.getX(idx), (int)event.getY(idx));
					break;
				}
				case MotionEvent.ACTION_CANCEL:
					_scummvm.updateTouch(JACTION_CANCEL, 0, 0, 0);
					break;
			}

			//return _gd.onTouchEvent(event);
			return true;
		} else {
			// Deal with LINT warning "ScummVMEvents#onTouch should call View#performClick when a click is detected"
			switch (action) {
				case MotionEvent.ACTION_UP:
					_handler.removeMessages(MSG_LONG_TOUCH_EVENT);
					v.performClick();
					break;
				case MotionEvent.ACTION_DOWN:
					// fall through
				default:
					break;
			}

			// check if the event can be handled as a multitouch event
			if (_multitouchHelper.handleMotionEvent(event)) {
				_handler.removeMessages(MSG_LONG_TOUCH_EVENT);
				return true;
			}

			return _gd.onTouchEvent(event);
		}
	}

	// OnGestureListener
	@Override
	final public boolean onDown(MotionEvent e) {
//		Log.d(ScummVM.LOG_TAG, "SCUMMV-EVENTS-BASE - onDOWN MotionEvent");
		if (_touchMode != TOUCH_MODE_GAMEPAD) {
			_scummvm.pushEvent(JE_DOWN, (int)e.getX(), (int)e.getY(), 0, 0, 0, 0);
		}
		return true;
	}

	@Override
	final public boolean onFling(MotionEvent e1, MotionEvent e2,
									float velocityX, float velocityY) {
		//Log.d(ScummVM.LOG_TAG, String.format(Locale.ROOT, "onFling: %s -> %s (%.3f %.3f)",
		//										e1.toString(), e2.toString(),
		//										velocityX, velocityY));

//		Log.d(ScummVM.LOG_TAG, "onFling");
		_handler.removeMessages(MSG_LONG_TOUCH_EVENT);
		return true;
	}

	@Override
	final public void onLongPress(MotionEvent e) {
		// disabled, interferes with drag&drop
	}

	@Override
	final public boolean onScroll(MotionEvent e1, MotionEvent e2,
									float distanceX, float distanceY) {
		_handler.removeMessages(MSG_LONG_TOUCH_EVENT);
//		Log.d(ScummVM.LOG_TAG, "onScroll");
		if (_touchMode != TOUCH_MODE_GAMEPAD) {
			// typical use:
			// - move mouse cursor around (most traditional point and click games)
			// - mouse look (eg. Myst 3)
			_scummvm.pushEvent(JE_SCROLL, (int)e1.getX(), (int)e1.getY(),
							(int)e2.getX(), (int)e2.getY(), (int)distanceX, (int)distanceY);
		}
		return true;
	}

	@Override
	final public void onShowPress(MotionEvent e) {
//		Log.d(ScummVM.LOG_TAG, "onShowPress");
		_handler.removeMessages(MSG_LONG_TOUCH_EVENT);
		if (_touchMode != TOUCH_MODE_GAMEPAD && !_doubleTapMode) {
			// Schedule a Right click notification
			_handler.sendMessageAtTime(_handler.obtainMessage(MSG_LONG_TOUCH_EVENT, 0, 0), e.getDownTime() + 500);
			// Middle click
			_handler.sendMessageAtTime(_handler.obtainMessage(MSG_LONG_TOUCH_EVENT, 1, 0), e.getDownTime() + 1500);
		}
	}

	@Override
	final public boolean onSingleTapUp(MotionEvent e) {
//		Log.d(ScummVM.LOG_TAG, "onSingleTapUp");
		_handler.removeMessages(MSG_LONG_TOUCH_EVENT);
		if (_touchMode != TOUCH_MODE_GAMEPAD) {
			_scummvm.pushEvent(JE_TAP, (int)e.getX(), (int)e.getY(),
							(int)(e.getEventTime() - e.getDownTime()), 0, 0, 0);
		}
		return true;
	}

	// OnDoubleTapListener
	@Override
	final public boolean onDoubleTap(MotionEvent e) {
//		Log.d(ScummVM.LOG_TAG, "onDoubleTap");
		_doubleTapMode = true;
		_handler.removeMessages(MSG_LONG_TOUCH_EVENT);
		return true;
	}

	@Override
	final public boolean onDoubleTapEvent(MotionEvent e) {
		switch (e.getAction()) {
			case MotionEvent.ACTION_MOVE:
				//if the second tap hadn't been released and it's being moved
//				Log.d(ScummVM.LOG_TAG, "onDoubleTapEvent Moving X: " + Float.toString(e.getRawX()) + " Y: " + Float.toString(e.getRawY()));
				break;

			case MotionEvent.ACTION_UP:
//				Log.d(ScummVM.LOG_TAG, "onDoubleTapEvent Release!");
				//user released the screen
				_doubleTapMode = false;
				break;

			case MotionEvent.ACTION_DOWN:
//				Log.d(ScummVM.LOG_TAG, "onDoubleTapEvent DOWN!");
				break;

			default:
//				Log.d(ScummVM.LOG_TAG, "onDoubleTapEvent UNKNOWN!");
				break;
		}

		if (_touchMode != TOUCH_MODE_GAMEPAD) {
			_scummvm.pushEvent(JE_DOUBLE_TAP, (int)e.getX(), (int)e.getY(), e.getAction(), 0, 0, 0);
		}
		return true;
	}

	@Override
	final public boolean onSingleTapConfirmed(MotionEvent e) {
		// Note, timing thresholds for double tap detection seem to be hardcoded in the framework
		// as ViewConfiguration.getDoubleTapTimeout()
//		Log.d(ScummVM.LOG_TAG, "onSingleTapConfirmed - double tap failed");
		return true;
	}
}
