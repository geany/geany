Quick Guide for new translations
--------------------------------

If you would like to translate Geany into another language, have a look at the
language statistics page at [1] first to see if your desired language already
exists. If it already exists, please read the "Notes for updating translations"
section. Otherwise, get the SVN version of Geany, change to the po directory and
start the new translation with:

$ msginit -l ll_CC -o ll.po -i geany.pot

Fill in ll with the language code and CC with the country code. For example, to
translate Geany into Italian you would type:

$ msginit -l it_IT -o it.po -i geany.pot

This will create a file it.po. This file can be opened with a text editor
(e.g. Geany ;-)) or a graphical program like PoEdit [2]. There are also several
other GUI programs for working on translations.

You don't need to modify the file po/LINGUAS, it is regenerated automatically on
the next build.

When you have finished editing the file, check the file with:

$ msgfmt -c --check-accelerators=_ it.po

Please take care of menu accelerators(strings containing a "_"). The "_"
character should also be in your translation. Additionally, it would be nice if
these accelerators are not twice for two strings inside a dialog or sub menu.

You can also use intl_stats.sh, e.g. by running the following command in the top
source directory of Geany:

$ po/intl_stats.sh -a it

This will print some information about the Italian translation and checks for
menu accelerators.

When you have finished your work - which doesn't mean you finished the
translation, you will not have to work alone - send the file to the translation
mailing list [3] or directly to Frank Lanitz [4] and he will add the translation
to Geany then.

It is a good idea to let any translator and Frank know before you start or while
translating, because they can give you hints on translating and Frank can ensure
that a translation is not already in progress.


Notes for updating translations
-------------------------------

If you want to update an existing translation, please contact the translation
mailing list [3] and/or Frank Lanitz [4] directly. He is supervising all
translation issues and will contact the maintainer of the translation you want
to update to avoid any conflicts.

Some translation statistics can be found at:
http://i18n.geany.org/


I18n mailing list
-----------------

There is also a mailing list dedicated to translation issues. Please visit
http://www.geany.org/Support/MailingList#geany-i18 for more information.


[1] http://i18n.geany.org/
[2] http://www.poedit.net/
[3] http://www.geany.org/Support/MailingList#geany-i18
[4] Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
