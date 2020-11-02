// Note that the status screen needs to run in 'play' scope!

class InterBackground native play version("2.5")
{
	native static InterBackground Create(wbstartstruct wbst);
	native virtual bool LoadBackground(bool isenterpic);
	native virtual void updateAnimatedBack();
	native virtual void drawBackground(int CurState, bool drawsplat, bool snl_pointeron);
}

struct PatchInfo play version("2.5")
{
	Font mFont;
	TextureID mPatch;
	int mColor;

	void Init(GIFont gifont)
	{
		if (gifont.color == 'Null')
		{
			mPatch = TexMan.CheckForTexture(gifont.fontname, TexMan.Type_MiscPatch);
			mColor = mPatch.isValid() ? Font.CR_UNTRANSLATED : Font.CR_UNDEFINED;
			mFont = NULL;
		}
		else
		{
			mFont = Font.GetFont(gifont.fontname);
			mColor = Font.FindFontColor(gifont.color);
			mPatch.SetInvalid();
		}
		if (mFont == NULL)
		{
			mFont = BigFont;
		}
	}
};


// Will be made a class later, but for now needs to mirror the internal version.
class StatusScreen abstract play version("2.5")
{
	enum EValues
	{
		// GLOBAL LOCATIONS
		TITLEY = 2,

		// SINGPLE-PLAYER STUFF
		SP_STATSX = 50,
		SP_STATSY = 50,

		SP_TIMEX = 8,
		SP_TIMEY = (200 - 32),

		// NET GAME STUFF
		NG_STATSY = 50,
	};

	enum EState
	{
		NoState = -1,
		StatCount,
		ShowNextLoc,
		LeavingIntermission
	};

	// States for single-player
	enum ESPState
	{
		SP_KILLS = 0,
		SP_ITEMS = 2,
		SP_SECRET = 4,
		SP_FRAGS = 6,
		SP_TIME = 8,
	};

	const SHOWNEXTLOCDELAY = 4;			// in seconds

	InterBackground bg;
	int				acceleratestage;	// used to accelerate or skip a stage
	bool				playerready[MAXPLAYERS];
	int				me;					// wbs.pnum
	int				bcnt;
	int				CurState;				// specifies current CurState
	wbstartstruct	wbs;				// contains information passed into intermission
	wbplayerstruct	Plrs[MAXPLAYERS];				// wbs.plyr[]
	int				cnt;				// used for general timing
	int				cnt_kills[MAXPLAYERS];
	int				cnt_items[MAXPLAYERS];
	int				cnt_secret[MAXPLAYERS];
	int				cnt_frags[MAXPLAYERS];
	int				cnt_deaths[MAXPLAYERS];
	int				cnt_time;
	int				cnt_total_time;
	int				cnt_par;
	int				cnt_pause;
	int				total_frags;
	int				total_deaths;
	bool			noautostartmap;
	int				dofrags;
	int				ng_state;
	float			shadowalpha;

	PatchInfo 		mapname;
	PatchInfo 		finished;
	PatchInfo 		entering;
	PatchInfo		content;
	PatchInfo		author;

	TextureID 		p_secret;
	TextureID 		kills;
	TextureID 		secret;
	TextureID 		items;
	TextureID 		timepic;
	TextureID 		par;
	TextureID 		sucks;
	TextureID		finishedPatch;
	TextureID		enteringPatch;

	// [RH] Info to dynamically generate the level name graphics
	String			lnametexts[2];
	String			authortexts[2];

	bool 			snl_pointeron;

	int 			player_deaths[MAXPLAYERS];
	int  			sp_state;


	//====================================================================
	//
	// Draws a single character with a shadow
	//
	//====================================================================

	int DrawCharPatch(Font fnt, int charcode, int x, int y, int translation = Font.CR_UNTRANSLATED, bool nomove = false)
	{
		int width = fnt.GetCharWidth(charcode);
		screen.DrawChar(fnt, translation, x, y, charcode, nomove ? DTA_CleanNoMove : DTA_Clean, true);
		return x - width;
	}

	//====================================================================
	//
	// Draws a level name with the big font
	//
	// x is no longer passed as a parameter because the text is now broken into several lines
	// if it is too long
	//
	//====================================================================

	int DrawName(int y, TextureID tex, String levelname)
	{
		// draw <LevelName> 
		if (tex.isValid())
		{
			int w,h;
			[w, h] = TexMan.GetSize(tex);
			let size = TexMan.GetScaledSize(tex);
			screen.DrawTexture(tex, true, (screen.GetWidth() - size.X * CleanXfac) /2, y, DTA_CleanNoMove, true);
			if (h > 50)
			{ // Fix for Deus Vult II and similar wads that decide to make these hugely tall
			  // patches with vast amounts of empty space at the bottom.
				size.Y = TexMan.CheckRealHeight(tex) * size.Y / h;
			}
			return y + (h + BigFont.GetHeight()/4) * CleanYfac;
		}
		else if (levelname.Length() > 0)
		{
			int h = 0;
			int lumph = mapname.mFont.GetHeight() * CleanYfac;

			BrokenLines lines = mapname.mFont.BreakLines(levelname, screen.GetWidth() / CleanXfac);

			int count = lines.Count();
			for (int i = 0; i < count; i++)
			{
				screen.DrawText(mapname.mFont, mapname.mColor, (screen.GetWidth() - lines.StringWidth(i) * CleanXfac) / 2, y + h, lines.StringAt(i), DTA_CleanNoMove, true);
				h += lumph;
			}
			return y + h + lumph/4;
		}
		return 0;
	}

	//====================================================================
	//
	// Draws a level author's name with the big font
	//
	//====================================================================
	
	int DrawAuthor(int y, String levelname)
	{
		if (levelname.Length() > 0)
		{
			int h = 0;
			int lumph = author.mFont.GetHeight() * CleanYfac;
			
			BrokenLines lines = author.mFont.BreakLines(levelname, screen.GetWidth() / CleanXfac);
			
			int count = lines.Count();
			for (int i = 0; i < count; i++)
			{
				screen.DrawText(author.mFont, author.mColor, (screen.GetWidth() - lines.StringWidth(i) * CleanXfac) / 2, y + h, lines.StringAt(i), DTA_CleanNoMove, true);
				h += lumph;
			}
			return y + h;
		}
		return y;
	}
	
	//====================================================================
	//
	// Draws a text, either as patch or as string from the string table
	//
	//====================================================================

	int DrawPatchText(int y, PatchInfo pinfo, String stringname)
	{
		String string = Stringtable.Localize(stringname);
		int midx = screen.GetWidth() / 2;

		if (pinfo.mPatch.isValid())
		{
			let size = TexMan.GetScaledSize(pinfo.mPatch);
			screen.DrawTexture(pinfo.mPatch, true, midx - size.X * CleanXfac/2, y, DTA_CleanNoMove, true);
			return y + int(size.Y * CleanYfac);
		}
		else 
		{
			screen.DrawText(pinfo.mFont, pinfo.mColor, midx - pinfo.mFont.StringWidth(string) * CleanXfac/2, y, string, DTA_CleanNoMove, true);
			return y + pinfo.mFont.GetHeight() * CleanYfac;
		}
	}


	//====================================================================
	//
	// Draws a text, either as patch or as string from the string table
	//
	//====================================================================
	
	int DrawPatchOrText(int y, PatchInfo pinfo, TextureID patch, String stringname)
	{
		String string = Stringtable.Localize(stringname);
		int midx = screen.GetWidth() / 2;
		
		if (TexMan.OkForLocalization(patch, stringname))
		{
			let size = TexMan.GetScaledSize(patch);
			screen.DrawTexture(patch, true, midx - size.X * CleanXfac/2, y, DTA_CleanNoMove, true);
			return y + int(size.Y * CleanYfac);
		}
		else
		{
			screen.DrawText(pinfo.mFont, pinfo.mColor, midx - pinfo.mFont.StringWidth(string) * CleanXfac/2, y, string, DTA_CleanNoMove, true);
			return y + pinfo.mFont.GetHeight() * CleanYfac;
		}
	}


	//====================================================================
	//
	// Draws "<Levelname> Finished!"
	//
	// Either uses the specified patch or the big font
	// A level name patch can be specified for all games now, not just Doom.
	//
	//====================================================================

	virtual int drawLF ()
	{
		int y = TITLEY * CleanYfac;

		y = DrawName(y, wbs.LName0, lnametexts[0]);
	
		// Adjustment for different font sizes for map name and 'finished'.
		y -= ((mapname.mFont.GetHeight() - finished.mFont.GetHeight()) * CleanYfac) / 4;

		y = DrawAuthor(y, authortexts[0]);

		// draw "Finished!"
		if (y < (NG_STATSY - finished.mFont.GetHeight()*3/4) * CleanYfac)
		{
			// don't draw 'finished' if the level name is too tall
			y = DrawPatchText(y, finished, "$WI_FINISHED");
		}
		return y;
	}


	//====================================================================
	//
	// Draws "Entering <LevelName>"
	//
	// Either uses the specified patch or the big font
	// A level name patch can be specified for all games now, not just Doom.
	//
	//====================================================================

	virtual void drawEL ()
	{
		int y = TITLEY * CleanYfac;

		y = DrawPatchText(y, entering, "$WI_ENTERING");
		y += entering.mFont.GetHeight() * CleanYfac / 4;
		y = DrawName(y, wbs.LName1, lnametexts[1]);

		DrawAuthor(y, authortexts[1]);

	}


	//====================================================================
	//
	// Draws a number.
	// If digits > 0, then use that many digits minimum,
	//	otherwise only use as many as necessary.
	// x is the right edge of the number.
	// Returns new x position, that is, the left edge of the number.
	//
	//====================================================================
	int drawNum (Font fnt, int x, int y, int n, int digits, bool leadingzeros = true, int translation = Font.CR_UNTRANSLATED)
	{
		int fntwidth = fnt.StringWidth("3");
		String text;
		int len;
		bool nomove = fnt != IntermissionFont;

		if (nomove)
		{
			fntwidth *= CleanXfac;
		}
		text = String.Format("%d", n);
		len = text.Length();
		if (leadingzeros)
		{
			int filldigits = digits - len;
			for(int i = 0; i < filldigits; i++)
			{
				text = "0" .. text;
			}
			len = text.Length();
		}
		
		for(int text_p = len-1; text_p >= 0; text_p--)
		{
			// Digits are centered in a box the width of the '3' character.
			// Other characters (specifically, '-') are right-aligned in their cell.
			int c = text.ByteAt(text_p);
			if (c >= "0" && c <= "9")
			{
				x -= fntwidth;
				DrawCharPatch(fnt, c, x + (fntwidth - fnt.GetCharWidth(c)) / 2, y, translation, nomove);
			}
			else
			{
				DrawCharPatch(fnt, c, x - fnt.GetCharWidth(c), y, translation, nomove);
				x -= fntwidth;
			}
		}
		if (len < digits)
		{
			x -= fntwidth * (digits - len);
		}
		return x;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	void drawPercent (Font fnt, int x, int y, int p, int b, bool show_total = true, int color = Font.CR_UNTRANSLATED)
	{
		if (p < 0)
			return;

		if (wi_percents)
		{
			if (fnt != IntermissionFont)
			{
				x -= fnt.StringWidth("%") * CleanXfac;
			}
			else
			{
				x -= fnt.StringWidth("%");
			}
			screen.DrawText(fnt, color, x, y, "%", fnt != IntermissionFont ? DTA_CleanNoMove : DTA_Clean, true);
			if (fnt != IntermissionFont)
			{
				x -= 2*CleanXfac;
			}
			drawNum(fnt, x, y, b == 0 ? 100 : p * 100 / b, -1, false, color);
		}
		else
		{
			if (show_total)
			{
				x = drawNum(fnt, x, y, b, 2, false);
				x -= fnt.StringWidth("/");
				screen.DrawText (IntermissionFont, color, x, y, "/", DTA_Clean, true);
			}
			drawNum (fnt, x, y, p, -1, false, color);
		}
	}

	//====================================================================
	//
	// Display level completion time and par, or "sucks" message if overflow.
	//
	//====================================================================

	void drawTime (int x, int y, int t, bool no_sucks=false)
	{
		bool sucky;

		if (t < 0)
			return;

		sucky = !no_sucks && t >= wbs.sucktime * 60 * 60 && wbs.sucktime > 0;

		if (sucky)
		{ // "sucks"
			if (Sucks.isValid())
			{
				let size = TexMan.GetScaledSize(Sucks);
				screen.DrawTexture (Sucks, true, x - size.X, y - size.Y - 2, DTA_Clean, true); 
			}
			else
			{
				screen.DrawText (BigFont, Font.CR_UNTRANSLATED, x  - BigFont.StringWidth("SUCKS"), y - IntermissionFont.GetHeight() - 2,	"SUCKS", DTA_Clean, true);
			}
		}

		int hours = t / 3600;
		t -= hours * 3600;
		int minutes = t / 60;
		t -= minutes * 60;
		int seconds = t;

		// Why were these offsets hard coded? Half the WADs with custom patches
		// I tested screwed up miserably in this function!
		int num_spacing = IntermissionFont.GetCharWidth("3");
		int colon_spacing = IntermissionFont.GetCharWidth(":");

		x = drawNum (IntermissionFont, x, y, seconds, 2) - 1;
		DrawCharPatch (IntermissionFont, ":", x -= colon_spacing, y);
		x = drawNum (IntermissionFont, x, y, minutes, 2, hours!=0);
		if (hours)
		{
			DrawCharPatch (IntermissionFont, ":", x -= colon_spacing, y);
			drawNum (IntermissionFont, x, y, hours, 2);
		}
	}

	void drawTimeFont (Font printFont, int x, int y, int t, int color) // hack hack
	{
		drawTime(x, y, t);
	}


	//====================================================================
	//
	//
	//
	//====================================================================

	virtual void End ()
	{
		CurState = LeavingIntermission;

		//Added by mc
		if (deathmatch)
		{
			level.RemoveAllBots (consoleplayer != Net_Arbitrator);
		}
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	bool autoSkip()
	{
		return wi_autoadvance > 0 && bcnt > (wi_autoadvance * Thinker.TICRATE);
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void initNoState ()
	{
		CurState = NoState;
		acceleratestage = 0;
		cnt = 10;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void updateNoState ()
	{
		if (acceleratestage)
		{
			cnt = 0;
		}
		else
		{
			bool noauto = noautostartmap;

			for (int i = 0; !noauto && i < MAXPLAYERS; ++i)
			{
				if (playeringame[i])
				{
					noauto |= players[i].GetNoAutostartMap();
				}
			}
			if (!noauto || autoSkip())
			{
				cnt--;
			}
		}

		if (cnt == 0)
		{
			End();
			level.WorldDone();
		}
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void initShowNextLoc ()
	{
		if (wbs.next == "") 
		{
			// Last map in episode - there is no next location!
			End();
			level.WorldDone();
			return;
		}

		CurState = ShowNextLoc;
		acceleratestage = 0;
		cnt = SHOWNEXTLOCDELAY * Thinker.TICRATE;
		noautostartmap = bg.LoadBackground(true);
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void updateShowNextLoc ()
	{
		if (!--cnt || acceleratestage)
			initNoState();
		else
			snl_pointeron = (cnt & 31) < 20;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void drawShowNextLoc(void)
	{
		bg.drawBackground(CurState, true, snl_pointeron);

		// draws which level you are entering..
		drawEL ();  

	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void drawNoState ()
	{
		snl_pointeron = true;
		drawShowNextLoc();
	}
	
	//====================================================================
	//
	//
	//
	//====================================================================

	protected int fragSum (int playernum)
	{
		int i;
		int frags = 0;
	
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i]
				&& i!=playernum)
			{
				frags += Plrs[playernum].frags[i];
			}
		}
		
		// JDC hack - negative frags.
		frags -= Plrs[playernum].frags[playernum];

		return frags;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	static void PlaySound(Sound snd)
	{
		S_StartSound(snd, CHAN_VOICE, CHANF_MAYBE_LOCAL|CHANF_UI, 1, ATTN_NONE);
	}
	
	
	// ====================================================================
	// checkForAccelerate
	// Purpose: See if the player has hit either the attack or use key
	//          or mouse button.  If so we set acceleratestage to 1 and
	//          all those display routines above jump right to the end.
	// Args:    none
	// Returns: void
	//
	// ====================================================================

	protected void checkForAccelerate(void)
	{
		int i;

		// check for button presses to skip delays
		for (i = 0; i < MAXPLAYERS; i++)
		{
			PlayerInfo player = players[i];
			if (playeringame[i])
			{
				if ((player.cmd.buttons ^ player.oldbuttons) &&
					((player.cmd.buttons & player.oldbuttons) == player.oldbuttons) && player.Bot == NULL)
				{
					acceleratestage = 1;
					playerready[i] = true;
				}
				player.oldbuttons = player.buttons;
			}
		}
	}
	
	// ====================================================================
	// Ticker
	// Purpose: Do various updates every gametic, for stats, animation,
	//          checking that intermission music is running, etc.
	// Args:    none
	// Returns: void
	//
	// ====================================================================
	
	virtual void StartMusic()
	{
		level.SetInterMusic(wbs.next);
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	virtual void Ticker(void)
	{
		// counter for general background animation
		bcnt++;  
	
		if (bcnt == 1)
		{
			StartMusic();
		}
	
		checkForAccelerate();
		bg.updateAnimatedBack();
	
		switch (CurState)
		{
		case StatCount:
			updateStats();
			break;
		
		case ShowNextLoc:
			updateShowNextLoc();
			break;
		
		case NoState:
			updateNoState();
			break;

		case LeavingIntermission:
			// Hush, GCC.
			break;
		}
	}
	
	//====================================================================
	//
	//
	//
	//====================================================================

	virtual void Drawer (void)
	{
		switch (CurState)
		{
		case StatCount:
			// draw animated background
			bg.drawBackground(CurState, false, false);
			drawStats();
			break;
	
		case ShowNextLoc:
		case LeavingIntermission:	// this must still draw the screen once more for the wipe code to pick up.
			drawShowNextLoc();
			break;
	
		default:
			drawNoState();
			break;
		}
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	virtual void Start (wbstartstruct wbstartstruct)
	{
		wbs = wbstartstruct;
		acceleratestage = 0;
		cnt = bcnt = 0;
		me = wbs.pnum;
		for (int i = 0; i < MAXPLAYERS; i++) Plrs[i] = wbs.plyr[i];
		
		entering.Init(gameinfo.mStatscreenEnteringFont);
		finished.Init(gameinfo.mStatscreenFinishedFont);
		mapname.Init(gameinfo.mStatscreenMapNameFont);
		content.Init(gameinfo.mStatscreenContentFont);
		author.Init(gameinfo.mStatscreenAuthorFont);

		Kills = TexMan.CheckForTexture("WIOSTK", TexMan.Type_MiscPatch);		// "kills"
		Secret = TexMan.CheckForTexture("WIOSTS", TexMan.Type_MiscPatch);		// "scrt"
		P_secret = TexMan.CheckForTexture("WISCRT2", TexMan.Type_MiscPatch);	// "secret"
		Items = TexMan.CheckForTexture("WIOSTI", TexMan.Type_MiscPatch);		// "items"
		Timepic = TexMan.CheckForTexture("WITIME", TexMan.Type_MiscPatch);		// "time"
		Sucks = TexMan.CheckForTexture("WISUCKS", TexMan.Type_MiscPatch);		// "sucks"
		Par = TexMan.CheckForTexture("WIPAR", TexMan.Type_MiscPatch);			// "par"
		enteringPatch = TexMan.CheckForTexture("WIENTER", TexMan.Type_MiscPatch);	// "entering"
		finishedPatch = TexMan.CheckForTexture("WIF", TexMan.Type_MiscPatch);			// "finished"

		// Use the local level structure which can be overridden by hubs
		lnametexts[0] = level.LevelName;		
		lnametexts[1] = wbstartstruct.nextname;
		authortexts[0] = StringTable.Localize(wbstartstruct.thisauthor);
		authortexts[1] = StringTable.Localize(wbstartstruct.nextauthor);

		bg = InterBackground.Create(wbs);
		noautostartmap = bg.LoadBackground(false);
		initStats();
	}
	
	
	protected virtual void initStats() {}
	protected virtual void updateStats() {}
	protected virtual void drawStats() {}

	native static int, int, int GetPlayerWidths();
	native static Color GetRowColor(PlayerInfo player, bool highlight);
	native static void GetSortedPlayers(in out Array<int> sorted, bool teamplay);
}
