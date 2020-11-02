/*
** optionmenuitems.txt
** Control items for option menus
**
**---------------------------------------------------------------------------
** Copyright 2010-2017 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

class OptionMenuItem : MenuItemBase
{
	String mLabel;
	bool mCentered;
	
	protected void Init(String label, String command, bool center = false)
	{
		Super.Init(0, 0, command);
		mLabel = label;
		mCentered = center;
	}

	protected int drawLabel(int indent, int y, int color, bool grayed = false)
	{
		String label = Stringtable.Localize(mLabel);
		
		int overlay = grayed? Color(96,48,0,0) : 0;

		int x;
		int w = SmallFont.StringWidth(label) * CleanXfac_1;
		if (!mCentered) x = indent - w;
		else x = (screen.GetWidth() - w) / 2;
		screen.DrawText (SmallFont, color, x, y, label, DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay);
		return x;
	}
	
	int CursorSpace()
	{
		return (14 * CleanXfac_1);
	}
	
	override bool Selectable()
	{
		return true;
	}
	
	override int GetIndent()
	{
		if (mCentered) return 0;
		return SmallFont.StringWidth(Stringtable.Localize(mLabel));
	}
	
	override bool MouseEvent(int type, int x, int y)
	{
		if (Selectable() && type == Menu.MOUSE_Release)
		{
			return Menu.GetCurrentMenu().MenuEvent(Menu.MKEY_Enter, true);
		}
		return false;
	}
}

//=============================================================================
//
// opens a submenu, command is a submenu name
//
//=============================================================================

class OptionMenuItemSubmenu : OptionMenuItem
{
	int mParam;
	OptionMenuItemSubmenu Init(String label, Name command, int param = 0, bool centered = false)
	{
		Super.init(label, command, centered);
		mParam = param;
		return self;
	}

	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		int x = drawLabel(indent, y, selected? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColorMore);
		if (mCentered) 
		{
			return x - 16*CleanXfac_1;
		}
		return indent;
	}

	override bool Activate()
	{
		Menu.MenuSound("menu/choose");
		Menu.SetMenu(mAction, mParam);
		return true;
	}
}

//=============================================================================
//
// opens a submenu, command is a submenu name
//
//=============================================================================

class OptionMenuItemLabeledSubmenu : OptionMenuItemSubmenu
{
	CVar mLabelCVar;
	OptionMenuItemSubmenu Init(String label, CVar labelcvar, Name command, int param = 0)
	{
		Super.init(label, command, false);
		mLabelCVar = labelcvar;
		return self;
	}

	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, selected? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor);
		
		String text = mLabelCVar.GetString();
		if (text.Length() == 0) text = Stringtable.Localize("$notset");
		screen.DrawText (SmallFont, OptionMenuSettings.mFontColorValue, indent + CursorSpace(), y, text, DTA_CleanNoMove_1, true);
		
		return indent;
	}
}

//=============================================================================
//
// Executes a CCMD, command is a CCMD name
//
//=============================================================================

class OptionMenuItemCommand : OptionMenuItemSubmenu
{
	private String ccmd;	// do not allow access to this from the outside.
	bool mCloseOnSelect;
	private bool mUnsafe;
	
	OptionMenuItemCommand Init(String label, Name command, bool centered = false, bool closeonselect = false)
	{
		Super.Init(label, command, 0, centered);
		ccmd = command;
		mCloseOnSelect = closeonselect;
		mUnsafe = true;
		return self;
	}

	private native static void DoCommand(String cmd, bool unsafe);	// This is very intentionally limited to this menu item to prevent abuse.

	override bool Activate()
	{
		// This needs to perform a few checks to prevent abuse by malicious modders.
		if (GetClass() != "OptionMenuItemSafeCommand")
		{
			let m = OptionMenu(Menu.GetCurrentMenu());
			// don't execute if no menu is active
			if (m == null) return false;	
			// don't execute if this item cannot be found in the current menu.
			if (m.GetItem(mAction) != self) return false;
		}
		else mUnsafe = false;
		Menu.MenuSound("menu/choose");
		DoCommand(ccmd, mUnsafe);
		if (mCloseOnSelect)
		{
			let curmenu = Menu.GetCurrentMenu();
			if (curmenu != null) curmenu.Close();
		}
		return true;
	}

}

//=============================================================================
//
// Executes a CCMD after confirmation, command is a CCMD name
//
//=============================================================================

class OptionMenuItemSafeCommand : OptionMenuItemCommand
{
	String mPrompt;


	OptionMenuItemSafeCommand Init(String label, Name command, String prompt = "")
	{
		Super.Init(label, command);
		mPrompt = prompt;
		return self;
	}

	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mkey == Menu.MKEY_MBYes)
		{
			Super.Activate();
			return true;
		}
		return Super.MenuEvent(mkey, fromcontroller);
	}

	override bool Activate()
	{
		String msg = mPrompt.Length() > 0 ? mPrompt : "$SAFEMESSAGE";
		msg = StringTable.Localize(msg);
		String actionLabel = StringTable.localize(mLabel);

		String FullString;
		FullString = String.Format("%s%s%s\n\n%s", TEXTCOLOR_WHITE, actionLabel, TEXTCOLOR_NORMAL, msg);
		Menu.StartMessage(FullString, 0);
		return true;
	}
}

//=============================================================================
//
// Base class for option lists
//
//=============================================================================

class OptionMenuItemOptionBase : OptionMenuItem
{
	// command is a CVAR
	Name mValues;	// Entry in OptionValues table
	CVar mGrayCheck;
	int mCenter;
	
	const OP_VALUES = 0x11001;

	protected void Init(String label, Name command, Name values, CVar graycheck, int center)
	{
		Super.Init(label, command);
		mValues = values;
		mGrayCheck = graycheck;
		mCenter = center;
	}

	override bool SetString(int i, String newtext)
	{
		if (i == OP_VALUES) 
		{
			int cnt = OptionValues.GetCount(mValues);
			if (cnt >= 0) 
			{
				mValues = newtext;
				int s = GetSelection();
				if (s >= cnt) s = 0;
				SetSelection(s);	// readjust the CVAR if its value is outside the range now
				return true;
			}
		}
		return false;
	}

	//=============================================================================
	virtual int GetSelection()
	{
		return 0;
	}
	
	virtual void SetSelection(int Selection)
	{
	}
	
	//=============================================================================
	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		bool grayed = isGrayed();

		if (mCenter)
		{
			indent = (screen.GetWidth() / 2);
		}
		drawLabel(indent, y, selected? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor, grayed);

		int overlay = grayed? Color(96,48,0,0) : 0;
		int Selection = GetSelection();
		String text = StringTable.Localize(OptionValues.GetText(mValues, Selection));
		if (text.Length() == 0) text = "Unknown";
		screen.DrawText (SmallFont, OptionMenuSettings.mFontColorValue, indent + CursorSpace(), y, text, DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay);
		return indent;
	}

	//=============================================================================
	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 0)
		{
			int Selection = GetSelection();
			if (mkey == Menu.MKEY_Left)
			{
				if (Selection == -1) Selection = 0;
				else if (--Selection < 0) Selection = cnt - 1;
			}
			else if (mkey == Menu.MKEY_Right || mkey == Menu.MKEY_Enter)
			{
				if (++Selection >= cnt) Selection = 0;
			}
			else
			{
				return Super.MenuEvent(mkey, fromcontroller);
			}
			SetSelection(Selection);
			Menu.MenuSound("menu/change");
		}
		else
		{
			return Super.MenuEvent(mkey, fromcontroller);
		}
		return true;
	}
	
	virtual bool isGrayed()
	{
		return mGrayCheck != null && !mGrayCheck.GetInt();
	}

	override bool Selectable()
	{
		return !isGrayed();
	}
}

//=============================================================================
//
// Change a CVAR, command is the CVAR name
//
//=============================================================================

class OptionMenuItemOption : OptionMenuItemOptionBase
{
	CVar mCVar;

	OptionMenuItemOption Init(String label, Name command, Name values, CVar graycheck = null, int center = 0)
	{
		Super.Init(label, command, values, graycheck, center);
		mCVar = CVar.FindCVar(mAction);
		return self;
	}

	//=============================================================================
	override int GetSelection()
	{
		int Selection = -1;
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 0 && mCVar != null)
		{
			if (OptionValues.GetTextValue(mValues, 0).Length() == 0)
			{
				let f = mCVar.GetFloat();
				for(int i = 0; i < cnt; i++)
				{ 
					if (f ~== OptionValues.GetValue(mValues, i))
					{
						Selection = i;
						break;
					}
				}
			}
			else
			{
				String cv = mCVar.GetString();
				for(int i = 0; i < cnt; i++)
				{
					if (cv ~== OptionValues.GetTextValue(mValues, i))
					{
						Selection = i;
						break;
					}
				}
			}
		}
		return Selection;
	}

	override void SetSelection(int Selection)
	{
		int cnt = OptionValues.GetCount(mValues);
		if (cnt > 0 && mCVar != null)
		{
			if (OptionValues.GetTextValue(mValues, 0).Length() == 0)
			{
				mCVar.SetFloat(OptionValues.GetValue(mValues, Selection));
			}
			else
			{
				mCVar.SetString(OptionValues.GetTextValue(mValues, Selection));
			}
		}
	}
}

//=============================================================================
//
// This class is used to capture the key to be used as the new key binding
// for a control item
//
//=============================================================================

class EnterKey : Menu
{
	OptionMenuItemControlBase mOwner;

	void Init(Menu parent, OptionMenuItemControlBase owner)
	{
		Super.Init(parent);
		mOwner = owner;
		SetMenuMessage(1);
		menuactive = Menu.WaitKey;	// There should be a better way to disable GUI capture...
	}

	override bool TranslateKeyboardEvents()
	{
		return false; 
	}

	private void SetMenuMessage(int which)
	{
		let parent = OptionMenu(mParentMenu);
		if (parent != null)
		{
			let it = parent.GetItem('Controlmessage');
			if (it != null)
			{
				it.SetValue(0, which);
			}
		}
	}

	override bool OnInputEvent(InputEvent ev)
	{
		// This menu checks raw keys, not GUI keys because it needs the raw codes for binding.
		if (ev.type == InputEvent.Type_KeyDown)
		{
			mOwner.SendKey(ev.KeyScan);
			menuactive = Menu.On;
			SetMenuMessage(0);
			Close();
			mParentMenu.MenuEvent((ev.KeyScan == InputEvent.KEY_ESCAPE)? Menu.MKEY_Abort : Menu.MKEY_Input, 0);
			return true;
		}
		return false;
	}

	override void Drawer()
	{
		mParentMenu.Drawer();
	}
}

//=============================================================================
//
// // Edit a key binding, Action is the CCMD to bind
//
//=============================================================================

class OptionMenuItemControlBase : OptionMenuItem
{
	KeyBindings mBindings;
	int mInput;
	bool mWaiting;

	protected void Init(String label, Name command, KeyBindings bindings)
	{
		Super.init(label, command);
		mBindings = bindings;
		mWaiting = false;
	}

	//=============================================================================
	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, mWaiting? OptionMenuSettings.mFontColorHighlight: 
			(selected? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor));

		String description;
		int Key1, Key2;

		[Key1, Key2] = mBindings.GetKeysForCommand(mAction);
		description = KeyBindings.NameKeys (Key1, Key2);
		if (description.Length() > 0)
		{
			Menu.DrawConText(Font.CR_WHITE, indent + CursorSpace(), y + (OptionMenuSettings.mLinespacing-8)*CleanYfac_1, description);
		}
		else
		{
			screen.DrawText(SmallFont, Font.CR_BLACK, indent + CursorSpace(), y + (OptionMenuSettings.mLinespacing-8)*CleanYfac_1, "---", DTA_CleanNoMove_1, true);
		}
		return indent;
	}

	//=============================================================================
	override bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (mkey == Menu.MKEY_Input)
		{
			mWaiting = false;
			mBindings.SetBind(mInput, mAction);
			return true;
		}
		else if (mkey == Menu.MKEY_Clear)
		{
			mBindings.UnbindACommand(mAction);
			return true;
		}
		else if (mkey == Menu.MKEY_Abort)
		{
			mWaiting = false;
			return true;
		}
		return false;
	}
	
	void SendKey(int key)
	{
		mInput = key;
	}

	override bool Activate()
	{
		Menu.MenuSound("menu/choose");
		mWaiting = true;
		let input = new("EnterKey");
		input.Init(Menu.GetCurrentMenu(), self);
		input.ActivateMenu();
		return true;
	}
}

class OptionMenuItemControl : OptionMenuItemControlBase
{
	OptionMenuItemControl Init(String label, Name command)
	{
		Super.Init(label, command, Bindings);
		return self;
	}
}

class OptionMenuItemMapControl : OptionMenuItemControlBase
{
	OptionMenuItemMapControl Init(String label, Name command)
	{
		Super.Init(label, command, AutomapBindings);
		return self;
	}
}

//=============================================================================
//
//
//
//=============================================================================

class OptionMenuItemStaticText : OptionMenuItem
{
	int mColor;

	// this function is only for use from MENUDEF, it needs to do some strange things with the color for backwards compatibility.
	OptionMenuItemStaticText Init(String label, int cr = -1)
	{
		Super.Init(label, 'None', true);
		mColor = OptionMenuSettings.mFontColor;
		if ((cr & 0xffff0000) == 0x12340000) mColor = cr & 0xffff;
		else if (cr > 0) mColor = OptionMenuSettings.mFontColorHeader;
		return self;
	}

	OptionMenuItemStaticText InitDirect(String label, int cr)
	{
		Super.Init(label, 'None', true);
		mColor = cr;
		return self;
	}

	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, mColor);
		return -1;
	}

	override bool Selectable()
	{
		return false;
	}

}

//=============================================================================
//
//
//
//=============================================================================

class OptionMenuItemStaticTextSwitchable : OptionMenuItem
{
	int mColor;
	String mAltText;
	int mCurrent;

	// this function is only for use from MENUDEF, it needs to do some strange things with the color for backwards compatibility.
	OptionMenuItemStaticTextSwitchable Init(String label, String label2, Name command, int cr = -1)
	{
		Super.Init(label, command, true);
		mAltText = label2;
		mCurrent = 0;

		mColor = OptionMenuSettings.mFontColor;
		if ((cr & 0xffff0000) == 0x12340000) mColor = cr & 0xffff;
		else if (cr > 0) mColor = OptionMenuSettings.mFontColorHeader;
		return self;
	}

	OptionMenuItemStaticTextSwitchable InitDirect(String label, String label2, Name command, int cr)
	{
		Super.Init(label, command, true);
		mColor = cr;
		mAltText = label2;
		mCurrent = 0;
		return self;
	}

	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		String txt = StringTable.Localize(mCurrent? mAltText : mLabel);
		int w = SmallFont.StringWidth(txt) * CleanXfac_1;
		int x = (screen.GetWidth() - w) / 2;
		screen.DrawText (SmallFont, mColor, x, y, txt, DTA_CleanNoMove_1, true);
		return -1;
	}

	override bool SetValue(int i, int val)
	{
		if (i == 0) 
		{
			mCurrent = val;
			return true;
		}
		return false;
	}

	override bool SetString(int i, String newtext)
	{
		if (i == 0) 
		{
			mAltText = newtext;
			return true;
		}
		return false;
	}

	override bool Selectable()
	{
		return false;
	}
}

//=============================================================================
//
//
//
//=============================================================================

class OptionMenuSliderBase : OptionMenuItem
{
	// command is a CVAR
	double mMin, mMax, mStep;
	int mShowValue;
	int mDrawX;
	int mSliderShort;

	protected void Init(String label, double min, double max, double step, int showval, Name command = 'none')
	{
		Super.Init(label, command);
		mMin = min;
		mMax = max;
		mStep = step;
		mShowValue = showval;
		mDrawX = 0;
		mSliderShort = 0;
	}

	virtual double GetSliderValue()
	{
		return 0;
	}
	
	virtual void SetSliderValue(double val)
	{
	}

	//=============================================================================
	//
	// Draw a slider. Set fracdigits negative to not display the current value numerically.
	//
	//=============================================================================

	protected void DrawSlider (int x, int y, double min, double max, double cur, int fracdigits, int indent)
	{
		String formater = String.format("%%.%df", fracdigits);	// The format function cannot do the '%.*f' syntax.
		String textbuf;
		double range;
		int maxlen = 0;
		int right = x + (12*8 + 4) * CleanXfac_1;
		int cy = y + (OptionMenuSettings.mLinespacing-8)*CleanYfac_1;

		range = max - min;
		double ccur = clamp(cur, min, max) - min;

		if (fracdigits >= 0)
		{
			textbuf = String.format(formater, max);
			maxlen = SmallFont.StringWidth(textbuf) * CleanXfac_1;
		}

		mSliderShort = right + maxlen > screen.GetWidth();

		if (!mSliderShort)
		{
			Menu.DrawConText(Font.CR_WHITE, x, cy, "\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12");
			Menu.DrawConText(Font.FindFontColor(gameinfo.mSliderColor), x + int((5 + ((ccur * 78) / range)) * CleanXfac_1), cy, "\x13");
		}
		else
		{
			// On 320x200 we need a shorter slider
			Menu.DrawConText(Font.CR_WHITE, x, cy, "\x10\x11\x11\x11\x11\x11\x12");
			Menu.DrawConText(Font.FindFontColor(gameinfo.mSliderColor), x + int((5 + ((ccur * 38) / range)) * CleanXfac_1), cy, "\x13");
			right -= 5*8*CleanXfac_1;
		}

		if (fracdigits >= 0 && right + maxlen <= screen.GetWidth())
		{
			textbuf = String.format(formater, cur);
			screen.DrawText(SmallFont, Font.CR_DARKGRAY, right, y, textbuf, DTA_CleanNoMove_1, true);
		}
	}


	//=============================================================================
	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, selected? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor);
		mDrawX = indent + CursorSpace();
		DrawSlider (mDrawX, y, mMin, mMax, GetSliderValue(), mShowValue, indent);
		return indent;
	}

	//=============================================================================
	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		double value = GetSliderValue();

		if (mkey == Menu.MKEY_Left)
		{
			value -= mStep;
		}
		else if (mkey == Menu.MKEY_Right)
		{
			value += mStep;
		}
		else
		{
			return OptionMenuItem.MenuEvent(mkey, fromcontroller);
		}
		if (value ~== 0) value = 0;	// This is to prevent formatting anomalies with very small values
		SetSliderValue(clamp(value, mMin, mMax));
		Menu.MenuSound("menu/change");
		return true;
	}

	override bool MouseEvent(int type, int x, int y)
	{
		let lm = OptionMenu(Menu.GetCurrentMenu());
		if (type != Menu.MOUSE_Click)
		{
			if (!lm.CheckFocus(self)) return false;
		}
		if (type == Menu.MOUSE_Release)
		{
			lm.ReleaseFocus();
		}

		int slide_left = mDrawX+8*CleanXfac_1;
		int slide_right = slide_left + (10*8*CleanXfac_1 >> mSliderShort);	// 12 char cells with 8 pixels each.

		if (type == Menu.MOUSE_Click)
		{
			if (x < slide_left || x >= slide_right) return true;
		}

		x = clamp(x, slide_left, slide_right);
		double v = mMin + ((x - slide_left) * (mMax - mMin)) / (slide_right - slide_left);
		if (v != GetSliderValue())
		{
			SetSliderValue(v);
			//Menu.MenuSound("menu/change");
		}
		if (type == Menu.MOUSE_Click)
		{
			lm.SetFocus(self);
		}
		return true;
	}

}

//=============================================================================
//
//
//
//=============================================================================

class OptionMenuItemSlider : OptionMenuSliderBase
{
	CVar mCVar;
	
	OptionMenuItemSlider Init(String label, Name command, double min, double max, double step, int showval = 1)
	{
		Super.Init(label, min, max, step, showval, command);
		mCVar =CVar.FindCVar(command);
		return self;
	}

	override double GetSliderValue()
	{
		if (mCVar != null)
		{
			return mCVar.GetFloat();
		}
		else
		{
			return 0;
		}
	}

	override void SetSliderValue(double val)
	{
		if (mCVar != null)
		{
			mCVar.SetFloat(val);
		}
	}
}

//=============================================================================
//
// // Edit a key binding, Action is the CCMD to bind
//
//=============================================================================

class OptionMenuItemColorPicker : OptionMenuItem
{
	CVar mCVar;

	const CPF_RESET = 0x20001;

	OptionMenuItemColorPicker Init(String label, Name command)
	{
		Super.Init(label, command);
		CVar cv = CVar.FindCVar(command);
		if (cv != null && cv.GetRealType() != CVar.CVAR_Color) cv = null;
		mCVar = cv;
		return self;
	}

	//=============================================================================
	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, selected? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor);

		if (mCVar != null)
		{
			int box_x = indent + CursorSpace();
			int box_y = y + CleanYfac_1;
			screen.Clear (box_x, box_y, box_x + 32*CleanXfac_1, box_y + OptionMenuSettings.mLinespacing*CleanYfac_1, mCVar.GetInt() | 0xff000000);
		}
		return indent;
	}

	override bool SetValue(int i, int v)
	{
		if (i == CPF_RESET && mCVar != null)
		{
			mCVar.ResetToDefault();
			return true;
		}
		return false;
	}

	override bool Activate()
	{
		if (mCVar != null)
		{
			Menu.MenuSound("menu/choose");
			
			// This code is a bit complicated because it should allow subclassing the
			// colorpicker menu.
			// New color pickers must inherit from the internal one to work here.
			
			let desc = MenuDescriptor.GetDescriptor('Colorpickermenu');
			if (desc != NULL && (desc.mClass == null || desc.mClass is "ColorPickerMenu"))
			{
				let odesc = OptionMenuDescriptor(desc);
				if (odesc != null)
				{
					let cls = desc.mClass;
					if (cls == null) cls = "ColorpickerMenu";
					let picker = ColorpickerMenu(new(cls));
					picker.Init(Menu.GetCurrentMenu(), mLabel, odesc, mCVar);
					picker.ActivateMenu();
					return true;
				}
			}
		}
		return false;
	}
}

class OptionMenuItemScreenResolution : OptionMenuItem
{
	String mResTexts[3];
	int mSelection;
	int mHighlight;
	int mMaxValid;

	enum EValues
	{
		SRL_INDEX = 0x30000,
		SRL_SELECTION = 0x30003,
		SRL_HIGHLIGHT = 0x30004,
	};

	OptionMenuItemScreenResolution Init(String command)
	{
		Super.Init("", command);
		mSelection = 0;
		mHighlight = -1;
		return self;
	}

	override bool SetValue(int i, int v)
	{
		if (i == SRL_SELECTION)
		{
			mSelection = v;
			return true;
		}
		else if (i == SRL_HIGHLIGHT)
		{
			mHighlight = v;
			return true;
		}
		return false;
	}

	override bool, int GetValue(int i)
	{
		if (i == SRL_SELECTION)
		{
			return true, mSelection;
		}
		return false, 0;
	}

	override bool SetString(int i, String newtext)
	{
		if (i >= SRL_INDEX && i <= SRL_INDEX+2) 
		{
			mResTexts[i-SRL_INDEX] = newtext;
			if (mResTexts[0].Length() == 0) mMaxValid = -1;
			else if (mResTexts[1].Length() == 0) mMaxValid = 0;
			else if (mResTexts[2].Length() == 0) mMaxValid = 1;
			else mMaxValid = 2;
			return true;
		}
		return false;
	}

	override bool, String GetString(int i)
	{
		if (i >= SRL_INDEX && i <= SRL_INDEX+2) 
		{
			return true, mResTexts[i-SRL_INDEX];
		}
		return false, "";
	}

	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mkey == Menu.MKEY_Left)
		{
			if (--mSelection < 0) mSelection = mMaxValid;
			Menu.MenuSound ("menu/cursor");
			return true;
		}
		else if (mkey == Menu.MKEY_Right)
		{
			if (++mSelection > mMaxValid) mSelection = 0;
			Menu.MenuSound ("menu/cursor");
			return true;
		}
		else 
		{
			return Super.MenuEvent(mkey, fromcontroller);
		}
		return false;
	}

	override bool MouseEvent(int type, int x, int y)
	{
		int colwidth = screen.GetWidth() / 3;
		mSelection = x / colwidth;
		return Super.MouseEvent(type, x, y);
	}

	override bool Activate()
	{
		Menu.MenuSound("menu/choose");
		Menu.SetVideoMode();
		return true;
	}

	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		int colwidth = screen.GetWidth() / 3;
		int col;

		for (int x = 0; x < 3; x++)
		{
			if (selected && mSelection == x)
				col = OptionMenuSettings.mFontColorSelection;
			else if (x == mHighlight)
				col = OptionMenuSettings.mFontColorHighlight;
			else
				col = OptionMenuSettings.mFontColorValue;

			screen.DrawText (SmallFont, col, colwidth * x + 20 * CleanXfac_1, y, mResTexts[x], DTA_CleanNoMove_1, true);
		}
		return colwidth * mSelection + 20 * CleanXfac_1 - CursorSpace();
	}

	override bool Selectable()
	{
		return mMaxValid >= 0;
	}

	override void Ticker()
	{
		if (Selectable() && mSelection > mMaxValid)
		{
			mSelection = mMaxValid;
		}
	}
}

//=============================================================================
//
// [TP] OptionMenuFieldBase
//
// Base class for input fields
//
//=============================================================================

class OptionMenuFieldBase : OptionMenuItem
{
	void Init (String label, Name command, CVar graycheck = null)
	{
		Super.Init(label, command);
		mCVar = CVar.FindCVar(mAction);
		mGrayCheck  = graycheck;
	}

	String GetCVarString()
	{
		if (mCVar == null)
			return "";

		return mCVar.GetString();
	}

	virtual String Represent()
	{
		return GetCVarString();
	}

	override int Draw (OptionMenuDescriptor d, int y, int indent, bool selected)
	{
		bool grayed = mGrayCheck != null && !mGrayCheck.GetInt();
		drawLabel(indent, y, selected ? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor, grayed);
		int overlay = grayed? Color(96, 48, 0, 0) : 0;

		screen.DrawText(SmallFont, OptionMenuSettings.mFontColorValue, indent + CursorSpace(), y, Represent(), DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay);
		return indent;
	}

	override bool, String GetString (int i)
	{
		if (i == 0)
		{
			return true, GetCVarString();
		}
		return false, "";
	}

	override bool SetString (int i, String s)
	{
		if (i == 0)
		{
			if (mCVar) mCVar.SetString(s);
			return true;
		}
		return false;
	}

	override bool Selectable()
	{
		return mGrayCheck == null || mGrayCheck.GetInt() != 0;
	}

	CVar mCVar;
	CVar mGrayCheck;
}

//=============================================================================
//
// [TP] OptionMenuTextField
//
// A text input field widget, for use with string CVars.
//
//=============================================================================

class OptionMenuItemTextField : OptionMenuFieldBase
{
	TextEnterMenu mEnter;
	
	OptionMenuItemTextField Init (String label, Name command, CVar graycheck = null)
	{
		Super.Init(label, command, graycheck);
		mEnter = null;
		return self;
	}

	override String Represent()
	{
		if (mEnter) return mEnter.GetText() .. SmallFont.GetCursor();
		else 
		{
			bool b;
			String s;
			[b, s] = GetString(0);
			return s;
		}
	}

	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		if (mEnter)
		{
			// reposition the text so that the cursor is visible when in entering mode.
			String text = Represent();
			int tlen = SmallFont.StringWidth(text) * CleanXfac_1;
			int newindent = screen.GetWidth() - tlen - CursorSpace();
			if (newindent < indent) indent = newindent;
		}
		return Super.Draw(desc, y, indent, selected);
	}

	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mkey == Menu.MKEY_Enter)
		{
			bool b;
			String s;
			[b, s] = GetString(0);
			Menu.MenuSound("menu/choose");
			mEnter = TextEnterMenu.OpenTextEnter(Menu.GetCurrentMenu(), SmallFont, s, -1, fromcontroller);
			mEnter.ActivateMenu();
			return true;
		}
		else if (mkey == Menu.MKEY_Input)
		{
			SetString(0, mEnter.GetText());
			mEnter = null;
			return true;
		}
		else if (mkey == Menu.MKEY_Abort)
		{
			mEnter = null;
			return true;
		}

		return Super.MenuEvent(mkey, fromcontroller);
	}
}


//=============================================================================
//
// [TP] FOptionMenuNumberField
//
// A numeric input field widget, for use with number CVars where sliders are inappropriate (i.e.
// where the user is interested in the exact value specifically)
//
//=============================================================================

class OptionMenuItemNumberField : OptionMenuFieldBase
{
	OptionMenuItemNumberField Init (String label, Name command, float minimum = 0, float maximum = 100, float step = 1, CVar graycheck = null)
	{
		Super.Init(label, command, graycheck);
		mMinimum = min(minimum, maximum);
		mMaximum = max(minimum, maximum);
		mStep = max(1, step);
		return self;
	}

	override String Represent()
	{
		if (mCVar == null) return "";
		return String.format("%.3f", mCVar.GetFloat());
	}


	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		if (mCVar)
		{
			float value = mCVar.GetFloat();

			if (mkey == Menu.MKEY_Left)
			{
				value -= mStep;
				if (value < mMinimum) value = mMaximum;
			}
			else if (mkey == Menu.MKEY_Right || mkey == Menu.MKEY_Enter)
			{
				value += mStep;
				if (value > mMaximum) value = mMinimum;
			}
			else
				return Super.MenuEvent(mkey, fromcontroller);

			mCVar.SetFloat(value);
			Menu.MenuSound("menu/change");
		}
		else return Super.MenuEvent(mkey, fromcontroller);

		return true;
	}

	float mMinimum;
	float mMaximum;
	float mStep;
}

//=============================================================================
//
// A special slider that displays plain text for special settings related
// to scaling.
//
//=============================================================================

class OptionMenuItemScaleSlider : OptionMenuItemSlider
{
	String TextZero;
	String TextNegOne;
	int mClickVal;
	
	OptionMenuItemScaleSlider Init(String label, Name command, double min, double max, double step, String zero, String negone = "")
	{
		Super.Init(label, command, min, max, step, 0);
		mCVar =CVar.FindCVar(command);
		TextZero = zero;
		TextNEgOne = negone;
		mClickVal = -10;
		return self;
	}

	//=============================================================================
	override int Draw(OptionMenuDescriptor desc, int y, int indent, bool selected)
	{
		drawLabel(indent, y, selected? OptionMenuSettings.mFontColorSelection : OptionMenuSettings.mFontColor);

		int Selection = int(GetSliderValue());
		if ((Selection == 0 || Selection == -1) && mClickVal <= 0)
		{
			String text = Selection == 0? TextZero : Selection == -1? TextNegOne  : "";
			screen.DrawText (SmallFont, OptionMenuSettings.mFontColorValue, indent + CursorSpace(), y, text, DTA_CleanNoMove_1, true);
		}
		else
		{
			mDrawX = indent + CursorSpace();
			DrawSlider (mDrawX, y, mMin, mMax, GetSliderValue(), mShowValue, indent);
		}
		return indent;
	}
	
	override bool MouseEvent(int type, int x, int y)
	{
		int value = int(GetSliderValue());
		switch (type)
		{
			case Menu.MOUSE_Click:
				mClickVal = value;
				if (value <= 0) return false;
				return Super.MouseEvent(type, x, y);
				
			case Menu.MOUSE_Move:
				if (mClickVal <= 0) return false;
				return Super.MouseEvent(type, x, y);
				
			case Menu.MOUSE_Release:
				if (mClickVal <= 0)
				{
					mClickVal = -10;
					SetSliderValue(value + 1);
					return true;
				}
				mClickVal = -10;
				return Super.MouseEvent(type, x, y);
		}
		return false;
	}
	
}

