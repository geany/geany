# For complete documentation of this file; please see Geany's main documentation
[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false;false
comment=0xd00000;0xffffff;false;false
commentline=0xd00000;0xffffff;false;false
number=0x007f00;0xffffff;false;false
operator=0x301010;0xffffff;false;false
identifier=0xa20000;0xffffff;false;false
wordinquote=0x7f007f;0xffffff;false;false
inquote=0x7f007f;0xffffff;false;false
substitution=0x111199;0xffffff;false;false
modifier=0x7f007f;0xffffff;false;false
expand=0x000000;0xffffff;false;false
wordtcl=0x111199;0xffffff;true;false
wordtk=0x7f0000;0xffffff;true;false
worditcl=0x111199;0xffffff;true;false
wordtkcmds=0x7f0000;0xffffff;true;false
wordexpand=0x7f0000;0xffffff;true;false


[keywords]
# all items must be in one line
tcl=after append array auto_execok auto_import auto_load auto_load_index auto_qualify beep bgerror binary break case catch cd clock close concat continue dde default echo else elseif encoding eof error eval exec exit expr fblocked fconfigure fcopy file fileevent flush for foreach format gets glob global history http if incr info interp join lappend lindex linsert list llength load loadTk lrange lreplace lsearch lset lsort memory msgcat namespace open package pid pkg::create pkg_mkIndex Platform-specific proc puts pwd re_syntax read regexp registry regsub rename resource return scan seek set socket source split string subst switch tclLog tclMacPkgSearch tclPkgSetup tclPkgUnknown tell time trace unknown unset update uplevel upvar variable vwait while
tk=bell bind bindtags bitmap button canvas checkbutton clipboard colors console cursors destroy entry event focus font frame grab grid image Inter-client keysyms label labelframe listbox lower menu menubutton message option options pack panedwindow photo place radiobutton raise scale scrollbar selection send spinbox text tk tk_chooseColor tk_chooseDirectory tk_dialog tk_focusNext tk_getOpenFile tk_messageBox tk_optionMenu tk_popup tk_setPalette tkerror tkvars tkwait toplevel winfo wish wm
itcl=@scope body class code common component configbody constructor define destructor hull import inherit itcl itk itk_component itk_initialize itk_interior itk_option iwidgets keep method private protected public
tkcommands=tk_bisque tk_chooseColor tk_dialog tk_focusFollowsMouse tk_focusNext tk_focusPrev tk_getOpenFile tk_getSaveFile tk_messageBox tk_optionMenu tk_popup tk_setPalette tk_textCopy tk_textCut tk_textPaste tkButtonAutoInvoke tkButtonDown tkButtonEnter tkButtonInvoke tkButtonLeave tkButtonUp tkCancelRepeat tkCheckRadioDown tkCheckRadioEnter tkCheckRadioInvoke tkColorDialog tkColorDialog_BuildDialog tkColorDialog_CancelCmd tkColorDialog_Config tkColorDialog_CreateSelector tkColorDialog_DrawColorScale tkColorDialog_EnterColorBar tkColorDialog_HandleRGBEntry tkColorDialog_HandleSelEntry tkColorDialog_InitValues tkColorDialog_LeaveColorBar tkColorDialog_MoveSelector tkColorDialog_OkCmd tkColorDialog_RedrawColorBars tkColorDialog_RedrawFinalColor tkColorDialog_ReleaseMouse tkColorDialog_ResizeColorBars tkColorDialog_RgbToX tkColorDialog_SetRGBValue tkColorDialog_StartMove tkColorDialog_XToRgb tkConsoleAbout tkConsoleBind tkConsoleExit tkConsoleHistory tkConsoleInit tkConsoleInsert tkConsoleInvoke tkConsoleOutput tkConsolePrompt tkConsoleSource tkDarken tkEntryAutoScan tkEntryBackspace tkEntryButton1 tkEntryClosestGap tkEntryGetSelection tkEntryInsert tkEntryKeySelect tkEntryMouseSelect tkEntryNextWord tkEntryPaste tkEntryPreviousWord tkEntrySeeInsert tkEntrySetCursor tkEntryTranspose tkEventMotifBindings tkFDGetFileTypes tkFirstMenu tkFocusGroup_BindIn tkFocusGroup_BindOut tkFocusGroup_Create tkFocusGroup_Destroy tkFocusGroup_In tkFocusGroup_Out tkFocusOK tkGenerateMenuSelect tkIconList tkIconList_Add tkIconList_Arrange tkIconList_AutoScan tkIconList_Btn1 tkIconList_Config tkIconList_Create tkIconList_CtrlBtn1 tkIconList_Curselection tkIconList_DeleteAll tkIconList_Double1 tkIconList_DrawSelection tkIconList_FocusIn tkIconList_FocusOut tkIconList_Get tkIconList_Goto tkIconList_Index tkIconList_Invoke tkIconList_KeyPress tkIconList_Leave1 tkIconList_LeftRight tkIconList_Motion1 tkIconList_Reset tkIconList_ReturnKey tkIconList_See tkIconList_Select tkIconList_Selection tkIconList_ShiftBtn1 tkIconList_UpDown tkListbox tkListboxAutoScan tkListboxBeginExtend tkListboxBeginSelect tkListboxBeginToggle tkListboxCancel tkListboxDataExtend tkListboxExtendUpDown tkListboxKeyAccel_Goto tkListboxKeyAccel_Key tkListboxKeyAccel_Reset tkListboxKeyAccel_Set tkListboxKeyAccel_Unset tkListboxMotion tkListboxSelectAll tkListboxUpDown tkMbButtonUp tkMbEnter tkMbLeave tkMbMotion tkMbPost tkMenuButtonDown tkMenuDownArrow tkMenuDup tkMenuEscape tkMenuFind tkMenuFindName tkMenuFirstEntry tkMenuInvoke tkMenuLeave tkMenuLeftArrow tkMenuMotion tkMenuNextEntry tkMenuNextMenu tkMenuRightArrow tkMenuUnpost tkMenuUpArrow tkMessageBox tkMotifFDialog tkMotifFDialog_ActivateDList tkMotifFDialog_ActivateFEnt tkMotifFDialog_ActivateFList tkMotifFDialog_ActivateSEnt tkMotifFDialog_BrowseDList tkMotifFDialog_BrowseFList tkMotifFDialog_BuildUI tkMotifFDialog_CancelCmd tkMotifFDialog_Config tkMotifFDialog_Create tkMotifFDialog_FileTypes tkMotifFDialog_FilterCmd tkMotifFDialog_InterpFilter tkMotifFDialog_LoadFiles tkMotifFDialog_MakeSList tkMotifFDialog_OkCmd tkMotifFDialog_SetFilter tkMotifFDialog_SetListMode tkMotifFDialog_Update tkPostOverPoint tkRecolorTree tkRestoreOldGrab tkSaveGrabInfo tkScaleActivate tkScaleButton2Down tkScaleButtonDown tkScaleControlPress tkScaleDrag tkScaleEndDrag tkScaleIncrement tkScreenChanged tkScrollButton2Down tkScrollButtonDown tkScrollButtonDrag tkScrollButtonUp tkScrollByPages tkScrollByUnits tkScrollDrag tkScrollEndDrag tkScrollSelect tkScrollStartDrag tkScrollTopBottom tkScrollToPos tkTabToWindow tkTearOffMenu tkTextAutoScan tkTextButton1 tkTextClosestGap tkTextInsert tkTextKeyExtend tkTextKeySelect tkTextNextPara tkTextNextPos tkTextNextWord tkTextPaste tkTextPrevPara tkTextPrevPos tkTextPrevWord tkTextResetAnchor tkTextScrollPages tkTextSelectTo tkTextSetCursor tkTextTranspose tkTextUpDownLine tkTraverseToMenu tkTraverseWithinMenu
expand=


[settings]
# the following characters are these which a "word" can contains, see documentation
wordchars=_#&abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# if only single comment char is supported like # in this file, leave comment_close blank
comment_open=#
comment_close=

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indention of the line, e.g. setting to true causes the following on pressing CTRL+d
	#command_example();
# setting to false would generate this
#	command_example();
# This setting works only for single line comments
comment_use_indent=true


[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
# (use only one of it at one time)
compiler=tclsh8.4 "%f"
run_cmd=tclsh8.4 "%f"

