;----------------------------------------------
; Bluefish Windows NSIS Install Script
;  Ukrainian Language Header
; 
;  The Bluefish Developers
;
;  Translators:
;   Yuri Chornoivan <yurchor@ukr.net>
;----------------------------------------------

; Section Names
!define SECT_BLUEFISH "Редактор Bluefish"
!define SECT_PLUGINS "Додатки"
!define SECT_SHORTCUT "Піктограма на стільниці"
!define SECT_DICT "Файли перевірки правопису (для звантаження потрібне з’єднання з мережею інтернет)"

; License Page
!define LICENSEPAGE_BUTTON "Далі"
!define LICENSEPAGE_FOOTER "${PRODUCT} випущено за умов дотримання GNU General Public License. Текст ліцензії наведено тут лише з інформаційною метою. $_CLICK"

; General Download Messages
; !define DOWN_LOCAL "Local copy of %s found..."
; !define DOWN_CHKSUM "Checksum verified..."
; !define DOWN_CHKSUM_ERROR "Checksum mismatch..."

; Aspell Strings
!define DICT_INSTALLED "Вже встановлено найсвіжішу версію цього словника, пропускаємо звантаження:"
!define DICT_DOWNLOAD "Звантаження словника перевірки правопису..."
!define DICT_FAILED "Помилка під час звантаження словника:"
!define DICT_EXTRACT "Видобування словника..."

; GTK+ Strings
!define GTK_DOWNLOAD "Звантаження GTK+..."
!define GTK_FAILED "Помилка під час звантаження GTK+:"
!define GTK_INSTALL "Встановлення GTK+..."
; !define GTK_UNINSTALL "Uninstalling GTK+..."
!define GTK_PATH "Встановлення GTK+ до каталогу програм системи."
; !define GTK_REQUIRED "Please install GTK+ ${GTK_MIN_VERSION} or higher and make sure it is in your PATH before running Bluefish."

; Plugin Names
!define PLUG_CHARMAP "Таблиця символів"
!define PLUG_ENTITIES "Об’єкти"
!define PLUG_HTMLBAR "Панель HTML"
!define PLUG_INFBROWSER "Панель перегляду даних"
!define PLUG_SNIPPETS "Фрагменти"
; !define PLUG_ZENCODING "Zencoding"

; File Associations Page
!define FA_TITLE "Прив’язка файлів"
!define FA_HEADER "Вкажіть типи файлів, для редагування яких типово слід використовувати ${PRODUCT}."
!define FA_SELECT "Позначити всі"
!define FA_UNSELECT "Зняти позначення з усіх"

; Misc
!define FINISHPAGE_LINK "Відвідати домашню сторінку Bluefish"
!define UNINSTALL_SHORTCUT "Вилучити ${PRODUCT}"
; !define FILETYPE_REGISTER "Registering File Type:"
; !define UNSTABLE_UPGRADE "An unstable release of ${PRODUCT} is installed.$\nShould previous versions be removed before we continue (Recommended)?"

; InetC Plugin Translations
;  /TRANSLATE downloading connecting second minute hour plural progress remaining
; !define INETC_DOWN "Downloading %s"
; !define INETC_CONN "Connecting ..."
; !define INETC_TSEC "second"
; !define INETC_TMIN "minute"
; !define INETC_THOUR "hour"
; !define INETC_TPLUR "s"
; !define INETC_PROGRESS "%dkB (%d%%) of %dkB @ %d.%01dkB/s"
; !define INETC_REMAIN " (%d %s%s remaining)"

; Content Types
!define CT_ADA	"Файл кодів Ada"
!define CT_ASP "Скрипт сторінки ActiveServer"
!define CT_SH	"Скрипт оболонки Bash"
!define CT_BFPROJECT	"Проект Bluefish"
!define CT_BFLANG2	"Файл визначення мов Bluefish версії 2"
!define CT_C	"Файл кодів мовою C"
!define CT_H	"Файл заголовків мовою C"
!define CT_CPP	"Файл кодів мовою C++"
!define CT_HPP	"Файл заголовків мовою C++"
!define CT_CSS "Таблиця стилів"
!define CT_D	"Файл кодів мовою D"
; !define CT_DIFF "Diff/Patch File"
!define CT_PO	"Файл перекладу Gettext"
!define CT_JAVA	"Файл кодів мовою Java"	
!define CT_JS	"Скрипт JavaScript"
!define CT_JSP	"Скрипт сторінок JavaServer"
; !define CT_MW	"MediaWiki File"
!define CT_NSI	"Скрипт NSIS"
!define CT_NSH	"Файл заголовків NSIS"
!define CT_PL	"Скрипт Perl"
!define CT_PHP	"Скрипт PHP"
; !define CT_INC	"PHP Include Script"
!define CT_TXT	"Звичайний текст"
!define CT_PY	"Скрипт Python"
!define CT_RB	"Скрипт Ruby"
!define CT_SMARTY	"Скрипт Smarty"
!define CT_VBS	"Скрипт VisualBasic"
!define CT_XHTML	"Скрипт XHTML"
!define CT_XML	"Файл XML"
!define CT_XSL	"Таблиця стилів XML"
