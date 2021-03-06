/* Initializes a new PO file.
   Copyright (C) 2001-2002 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2001.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "liballoca.h"

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <pwd.h>

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#if CLOSEDIR_VOID
/* Fake a return value. */
# define CLOSEDIR(d) (closedir (d), 0)
#else
# define CLOSEDIR(d) closedir (d)
#endif

#if HAVE_DIRENT_H || HAVE_NDIR_H || HAVE_SYS_DIR_H || HAVE_SYS_NDIR_H
# define HAVE_DIR 1
#else
# define HAVE_DIR 0
#endif

#include "error.h"
#include "progname.h"
#include "basename.h"
#include "strpbrk.h"
#include "message.h"
#include "read-po.h"
#include "write-po.h"
#include "po-charset.h"
#include "po-time.h"
#include "plural-table.h"
#include "xmalloc.h"
#include "exit.h"
#include "pathname.h"
#include "xerror.h"
#include "msgl-english.h"
#include "pipe.h"
#include "wait-process.h"
#include "getline.h"
#include "xsetenv.h"
#include "str-list.h"
#include "gettext.h"

#define _(str) gettext (str)
#define N_(str) (str)

#define SIZEOF(a) (sizeof(a) / sizeof(a[0]))

extern const char * locale_charset PARAMS ((void));
extern const char * _nl_locale_name PARAMS ((int category,
					     const char *categoryname));
extern const char * _nl_expand_alias PARAMS ((const char *name));

/* Locale name.  */
static const char *locale;

/* Language (ISO-639 code) and optional territory (ISO-3166 code).  */
static const char *catalogname;

/* Language (ISO-639 code).  */
static const char *language;

/* If true, the user is not considered to be the translator.  */
static bool no_translator;

/* Long options.  */
static const struct option long_options[] =
{
  { "help", no_argument, NULL, 'h' },
  { "input", required_argument, NULL, 'i' },
  { "locale", required_argument, NULL, 'l' },
  { "no-translator", no_argument, NULL, CHAR_MAX + 1 },
  { "no-wrap", no_argument, NULL, CHAR_MAX + 2 },
  { "output-file", required_argument, NULL, 'o' },
  { "version", no_argument, NULL, 'V' },
  { "width", required_argument, NULL, 'w' },
  { NULL, 0, NULL, 0 }
};

/* Prototypes for local functions.  Needed to ensure compiler checking of
   function argument counts despite of K&R C function definition syntax.  */
static void usage PARAMS ((int status))
#if defined __GNUC__ && ((__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2)
     __attribute__ ((noreturn))
#endif
;
static const char *find_pot PARAMS ((void));
static const char *catalogname_for_locale PARAMS ((const char *locale));
static const char *language_of_locale PARAMS ((const char *locale));
static const char *canonical_locale_charset PARAMS ((void));
static const char *englishname_of_language PARAMS ((void));
static const char *project_id PARAMS ((void));
static const char *project_id_version PARAMS ((void));
static const char *po_revision_date PARAMS ((const char *header));
static struct passwd *get_user_pwd PARAMS ((void));
static const char *get_user_fullname PARAMS ((void));
static const char *get_user_email PARAMS ((void));
static const char *last_translator PARAMS ((void));
static const char *language_team_address PARAMS ((void));
static const char *language_team PARAMS ((void));
static const char *mime_version PARAMS ((void));
static const char *content_type PARAMS ((void));
static const char *content_transfer_encoding PARAMS ((void));
static const char *plural_forms PARAMS ((void));
static char *get_field PARAMS ((const char *header, const char *field));
static char *put_field PARAMS ((const char *old_header, const char *field,
				const char *value));
static const char *get_title PARAMS ((void));
static const char *subst_string PARAMS ((const char *str,
					 unsigned int nsubst,
					 const char *(*subst)[2]));
static void subst_string_list PARAMS ((string_list_ty *slp,
				       unsigned int nsubst,
				       const char *(*subst)[2]));
static msgdomain_list_ty *fill_header PARAMS ((msgdomain_list_ty *mdlp));


int
main (argc, argv)
     int argc;
     char **argv;
{
  int opt;
  bool do_help;
  bool do_version;
  char *output_file;
  const char *input_file;
  msgdomain_list_ty *result;

  /* Set program name for messages.  */
  set_program_name (argv[0]);
  error_print_progname = maybe_print_progname;

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* Set default values for variables.  */
  do_help = false;
  do_version = false;
  output_file = NULL;
  input_file = NULL;
  locale = NULL;

  while ((opt = getopt_long (argc, argv, "hi:l:o:Vw:", long_options, NULL))
	 != EOF)
    switch (opt)
      {
      case '\0':		/* Long option.  */
	break;

      case 'h':
	do_help = true;
	break;

      case 'i':
	if (input_file != NULL)
	  {
	    error (EXIT_SUCCESS, 0, _("at most one input file allowed"));
	    usage (EXIT_FAILURE);
	  }
	input_file = optarg;
	break;

      case 'l':
	locale = optarg;
	break;

      case 'o':
	output_file = optarg;
	break;

      case 'V':
	do_version = true;
	break;

      case 'w':
	{
	  int value;
	  char *endp;
	  value = strtol (optarg, &endp, 10);
	  if (endp != optarg)
	    message_page_width_set (value);
	}
	break;

      case CHAR_MAX + 1:
	no_translator = true;
	break;

      case CHAR_MAX + 2: /* --no-wrap */
	message_page_width_ignore ();
	break;

      default:
	usage (EXIT_FAILURE);
	break;
      }

  /* Version information is requested.  */
  if (do_version)
    {
      printf ("%s (GNU %s) %s\n", basename (program_name), PACKAGE, VERSION);
      /* xgettext: no-wrap */
      printf (_("Copyright (C) %s Free Software Foundation, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"),
	      "2001-2002");
      printf (_("Written by %s.\n"), "Bruno Haible");
      exit (EXIT_SUCCESS);
    }

  /* Help is requested.  */
  if (do_help)
    usage (EXIT_SUCCESS);

  /* Test for extraneous arguments.  */
  if (optind != argc)
    error (EXIT_FAILURE, 0, _("too many arguments"));

  /* Search for the input file.  */
  if (input_file == NULL)
    input_file = find_pot ();

  /* Determine target locale.  */
  if (locale == NULL)
    {
      locale = _nl_locale_name (LC_MESSAGES, "LC_MESSAGES");
      if (strcmp (locale, "C") == 0)
	{
	  multiline_error (xstrdup (""),
			   xstrdup (_("\
You are in a language indifferent environment.  Please set\n\
your LANG environment variable, as described in the ABOUT-NLS\n\
file.  This is necessary so you can test your translations.\n")));
	  exit (EXIT_FAILURE);
	}
    }
  {
    const char *alias = _nl_expand_alias (locale);
    if (alias != NULL)
      locale = alias;
  }
  catalogname = catalogname_for_locale (locale);
  language = language_of_locale (locale);

  /* Default output file name is CATALOGNAME.po.  */
  if (output_file == NULL)
    {
      size_t cnlen = strlen (catalogname);

      output_file = (char *) xmalloc (cnlen + 3 + 1);
      memcpy (output_file, catalogname, cnlen);
      memcpy (output_file + cnlen, ".po", 3 + 1);

      /* But don't overwrite existing PO files.  */
      if (access (output_file, F_OK) == 0)
	{
	  multiline_error (xstrdup (""),
			   xasprintf (_("\
Output file %s already exists.\n\
Please specify the locale through the --locale option or\n\
the output .po file through the --output-file option.\n"),
				      output_file));
	  exit (EXIT_FAILURE);
	}
    }

  /* Read input file.  */
  result = read_po_file (input_file);

  /* Fill the header entry.  */
  result = fill_header (result);

  /* Initialize translations.  */
  if (strcmp (language, "en") == 0)
    result = msgdomain_list_english (result);

  /* Write the modified message list out.  */
  msgdomain_list_print (result, output_file, true, false);

  fprintf (stderr, "\n");
  fprintf (stderr, _("Created %s.\n"), output_file);

  exit (EXIT_SUCCESS);
}


/* Display usage information and exit.  */
static void
usage (status)
     int status;
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      /* xgettext: no-wrap */
      printf (_("\
Usage: %s [OPTION]\n\
"), program_name);
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Creates a new PO file, initializing the meta information with values from the\n\
user's environment.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Input file location:\n\
  -i, --input=INPUTFILE       input POT file\n\
If no input file is given, the current directory is searched for the POT file.\n\
If it is -, standard input is read.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Output file location:\n\
  -o, --output-file=FILE      write output to specified PO file\n\
If no output file is given, it depends on the --locale option or the user's\n\
locale setting.  If it is -, the results are written to standard output.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Output details:\n\
  -l, --locale=LL_CC          set target locale\n\
      --no-translator         assume the PO file is automatically generated\n\
  -w, --width=NUMBER          set output page width\n\
      --no-wrap               do not break long message lines, longer than\n\
                              the output page width, into several lines\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Informative output:\n\
  -h, --help                  display this help and exit\n\
  -V, --version               output version information and exit\n\
"));
      printf ("\n");
      fputs (_("Report bugs to <bug-gnu-gettext@gnu.org>.\n"),
	     stdout);
    }

  exit (status);
}


/* Search for the POT file and return its name.  */
static const char *
find_pot ()
{
#if HAVE_DIR
  DIR *dirp;
  char *found = NULL;

  dirp = opendir (".");
  if (dirp != NULL)
    {
      for (;;)
	{
	  struct dirent *dp;

	  errno = 0;
	  dp = readdir (dirp);
	  if (dp != NULL)
	    {
	      const char *name = dp->d_name;
	      size_t namlen = strlen (name);

	      if (namlen > 4 && memcmp (name + namlen - 4, ".pot", 4) == 0)
		{
		  if (found == NULL)
		    found = xstrdup (name);
		  else
		    {
		      multiline_error (xstrdup (""),
				       xstrdup (_("\
Found more than one .pot file.\n\
Please specify the input .pot file through the --input option.\n")));
		      usage (EXIT_FAILURE);
		    }
		}
	    }
	  else if (errno != 0)
	    error (EXIT_FAILURE, errno, _("error reading current directory"));
	  else
	    break;
	}
      if (CLOSEDIR (dirp))
	error (EXIT_FAILURE, errno, _("error reading current directory"));

      if (found != NULL)
	return found;
    }
#endif

  multiline_error (xstrdup (""),
		   xstrdup (_("\
Found no .pot file in the current directory.\n\
Please specify the input .pot file through the --input option.\n")));
  usage (EXIT_FAILURE);
  /* NOTREACHED */
  return NULL;
}


/* Return the gettext catalog name corresponding to a locale.  If the locale
   consists of a language and a territory, and the language is mainly spoken
   in that territory, the territory is removed from the locale name.
   For example, "de_DE" or "de_DE.ISO-8859-1" are simplified to "de",
   because the resulting catalog can be used as a default for all "de_XX",
   such as "de_AT".  */
static const char *
catalogname_for_locale (locale)
     const char *locale;
{
  static const char *locales_with_principal_territory[] = {
		/* Language	Main territory */
    "af_ZA",	/* Afrikaans	South Africa */
    "am_ET",	/* Amharic	Ethiopia */
    "as_IN",	/* Assamese	India */
    "az_AZ",	/* Azerbaijani	Azerbaijan */
    "be_BY",	/* Belarusian	Belarus */
    "bg_BG",	/* Bulgarian	Bulgaria */
    "bn_IN",	/* Bengali	India */
    "bo_CN",	/* Tibetan	China */
    "br_FR",	/* Breton	France */
    "bs_BA",	/* Bosnian	Bosnia */
    "ca_ES",	/* Catalan	Spain */
    "ce_RU",	/* Chechen	Russia */
    "co_FR",	/* Corsican	France */
    "cs_CZ",	/* Czech	Czech Republic */
    "cy_GB",	/* Welsh	Britain */
    "da_DK",	/* Danish	Denmark */
    "de_DE",	/* German	Germany */
    "dz_BT",	/* Dzongkha	Bhutan */
    "el_GR",	/* Greek	Greece */
    /* Don't put "en_GB" or "en_US" here.  That would be asking for fruitless
       political discussion.  */
    "es_ES",	/* Spanish	Spain */
    "et_EE",	/* Estonian	Estonia */
    "fa_IR",	/* Persian	Iran */
    "fi_FI",	/* Finnish	Finland */
    "fj_FJ",	/* Fijian	Fiji */
    "fo_FO",	/* Faroese	Faeroe Islands */
    "fr_FR",	/* French	France */
    "ga_IE",	/* Irish	Ireland */
    "gd_GB",	/* Scots	Britain */
    "gu_IN",	/* Gujarati	India */
    "he_IL",	/* Hebrew	Israel */
    "hi_IN",	/* Hindi	India */
    "hr_HR",	/* Croatian	Croatia */
    "hu_HU",	/* Hungarian	Hungary */
    "hy_AM",	/* Armenian	Armenia */
    "id_ID",	/* Indonesian	Indonesia */
    "is_IS",	/* Icelandic	Iceland */
    "it_IT",	/* Italian	Italy */
    "ja_JP",	/* Japanese	Japan */
    "jv_ID",	/* Javanese	Indonesia */
    "ka_GE",	/* Georgian	Georgia */
    "kk_KZ",	/* Kazakh	Kazakhstan */
    "kl_GL",	/* Kalaallisut	Greenland */
    "km_KH",	/* Khmer	Cambodia */
    "kn_IN",	/* Kannada	India */
    "ko_KR",	/* Korean	Korea (South) */
    "kok_IN",	/* Konkani	India */
    "lo_LA",	/* Laotian	Laos */
    "lt_LT",	/* Lithuanian	Lithuania */
    "lv_LV",	/* Latvian	Latvia */
    "mg_MG",	/* Malagasy	Madagascar */
    "mk_MK",	/* Macedonian	Macedonia */
    "ml_IN",	/* Malayalam	India */
    "mn_MN",	/* Mongolian	Mongolia */
    "mr_IN",	/* Marathi	India */
    "ms_MY",	/* Malay	Malaysia */
    "mt_MT",	/* Maltese	Malta */
    "my_MM",	/* Burmese	Myanmar */
    "mni_IN",	/* Manipuri	India */
    "na_NR",	/* Nauru	Nauru */
    "nb_NO",	/* Norwegian Bokmål	Norway */
    "ne_NP",	/* Nepali	Nepal */
    "nl_NL",	/* Dutch	Netherlands */
    "nn_NO",	/* Norwegian Nynorsk	Norway */
    "no_NO",	/* Norwegian	Norway */
    "oc_FR",	/* Occitan	France */
    "or_IN",	/* Oriya	India */
    "pa_IN",	/* Punjabi	India */
    "pl_PL",	/* Polish	Poland */
    "ps_AF",	/* Pashto	Afghanistan */
    "pt_PT",	/* Portuguese	Portugal */
    "rm_CH",	/* Rhaeto-Roman	Switzerland */
    "rn_BI",	/* Kirundi	Burundi */
    "ro_RO",	/* Romanian	Romania */
    "ru_RU",	/* Russian	Russia */
    "sa_IN",	/* Sanskrit	India */
    "sc_IT",	/* Sardinian	Italy */
    "sg_CF",	/* Sango	Central African Rep. */
    "si_LK",	/* Sinhalese	Sri Lanka */
    "sk_SK",	/* Slovak	Slovakia */
    "sl_SI",	/* Slovenian	Slovenia */
    "so_SO",	/* Somali	Somalia */
    "sq_AL",	/* Albanian	Albania */
    "sr_YU",	/* Serbian	Yugoslavia */
    "sv_SE",	/* Swedish	Sweden */
    "te_IN",	/* Telugu	India */
    "tg_TJ",	/* Tajik	Tajikistan */
    "th_TH",	/* Thai		Thailand */
    "tk_TM",	/* Turkmen	Turkmenistan */
    "tl_PH",	/* Tagalog	Philippines */
    "to_TO",	/* Tonga	Tonga */
    "tr_TR",	/* Turkish	Turkey */
    "uk_UA",	/* Ukrainian	Ukraine */
    "ur_PK",	/* Urdu		Pakistan */
    "uz_UZ",	/* Uzbek	Uzbekistan */
    "vi_VN",	/* Vietnamese	Vietnam */
    "wa_BE",	/* Walloon	Belgium */
    "wen_DE"	/* Sorbian	Germany */
  };
  const char *dot;
  size_t i;

  /* Remove the ".codeset" part from the locale.  */
  dot = strchr (locale, '.');
  if (dot != NULL)
    {
      const char *codeset_end;
      char *shorter_locale;

      codeset_end = strpbrk (dot + 1, "_@+,");
      if (codeset_end == NULL)
	codeset_end = dot + strlen (dot);

      shorter_locale = (char *) xmalloc (strlen (locale));
      memcpy (shorter_locale, locale, dot - locale);
      strcpy (shorter_locale + (dot - locale), codeset_end);
      locale = shorter_locale;
    }

  /* If the territory is the language's principal territory, drop it.  */
  for (i = 0; i < SIZEOF (locales_with_principal_territory); i++)
    if (strcmp (locale, locales_with_principal_territory[i]) == 0)
      {
	const char *language_end;
	size_t len;
	char *shorter_locale;

	language_end = strchr (locale, '_');
	if (language_end == NULL)
	  abort ();

	len = language_end - locale;
	shorter_locale = (char *) xmalloc (len + 1);
	memcpy (shorter_locale, locale, len);
	shorter_locale[len] = '\0';
	locale = shorter_locale;
	break;
      }

  return locale;
}


/* Return the language of a locale.  */
static const char *
language_of_locale (locale)
     const char *locale;
{
  const char *language_end;

  language_end = strpbrk (locale, "_.@+,");
  if (language_end != NULL)
    {
      size_t len;
      char *result;

      len = language_end - locale;
      result = (char *) xmalloc (len + 1);
      memcpy (result, locale, len);
      result[len] = '\0';

      return result;
    }
  else
    return locale;
}


/* Return the most likely desired charset for the PO file, as a portable
   charset name.  */
static const char *
canonical_locale_charset ()
{
  const char *tmp;
  char *old_LC_ALL;
  const char *charset;

  /* Save LC_ALL environment variable.  */

  tmp = getenv ("LC_ALL");
  old_LC_ALL = (tmp != NULL ? xstrdup (tmp) : NULL);

  xsetenv ("LC_ALL", locale, 1);

#ifdef HAVE_SETLOCALE
  if (setlocale (LC_ALL, "") == NULL)
    /* Nonexistent locale.  Use anything.  */
    charset = "";
  else
#endif
    /* Get the locale's charset.  */
    charset = locale_charset ();

  /* Restore LC_ALL environment variable.  */

  if (old_LC_ALL != NULL)
    xsetenv ("LC_ALL", old_LC_ALL, 1), free (old_LC_ALL);
  else
    unsetenv ("LC_ALL");

#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif

  /* Canonicalize it.  */
  charset = po_charset_canonicalize (charset);
  if (charset == NULL)
    charset = po_charset_ascii;

  return charset;
}


/* Return the English name of the language.  */
static const char *
englishname_of_language ()
{
  /* Derived from ISO 639.  */
  static struct { const char *code; const char *english; } table[] = {
    { "aa", "Afar" },
    { "ab", "Abkhazian" },
    { "ae", "Avestan" },
    { "af", "Afrikaans" },
    { "am", "Amharic" },
    { "ar", "Arabic" },
    { "as", "Assamese" },
    { "ay", "Aymara" },
    { "az", "Azerbaijani" },
    { "ba", "Bashkir" },
    { "be", "Belarusian" },
    { "bg", "Bulgarian" },
    { "bh", "Bihari" },
    { "bi", "Bislama" },
    { "bn", "Bengali" },
    { "bo", "Tibetan" },
    { "br", "Breton" },
    { "bs", "Bosnian" },
    { "ca", "Catalan" },
    { "ce", "Chechen" },
    { "ch", "Chamorro" },
    { "co", "Corsican" },
    { "cs", "Czech" },
    { "cu", "Church Slavic" },
    { "cv", "Chuvash" },
    { "cy", "Welsh" },
    { "da", "Danish" },
    { "de", "German" },
    { "dz", "Dzongkha" },
    { "el", "Greek" },
    { "en", "English" },
    { "eo", "Esperanto" },
    { "es", "Spanish" },
    { "et", "Estonian" },
    { "eu", "Basque" },
    { "fa", "Persian" },
    { "fi", "Finnish" },
    { "fj", "Fijian" },
    { "fo", "Faroese" },
    { "fr", "French" },
    { "fy", "Frisian" },
    { "ga", "Irish" },
    { "gd", "Scots" },
    { "gl", "Galician" },
    { "gn", "Guarani" },
    { "gu", "Gujarati" },
    { "gv", "Manx" },
    { "ha", "Hausa" },
    { "he", "Hebrew" },
    { "hi", "Hindi" },
    { "ho", "Hiri Motu" },
    { "hr", "Croatian" },
    { "hu", "Hungarian" },
    { "hy", "Armenian" },
    { "hz", "Herero" },
    { "ia", "Interlingua" },
    { "id", "Indonesian" },
    { "ie", "Interlingue" },
    { "ik", "Inupiak" },
    { "is", "Icelandic" },
    { "it", "Italian" },
    { "iu", "Inuktitut" },
    { "ja", "Japanese" },
    { "jw", "Javanese" },
    { "ka", "Georgian" },
    { "ki", "Kikuyu" },
    { "kj", "Kuanyama" },
    { "kk", "Kazakh" },
    { "kl", "Kalaallisut" },
    { "km", "Khmer" },
    { "kn", "Kannada" },
    { "ko", "Korean" },
    { "ks", "Kashmiri" },
    { "ku", "Kurdish" },
    { "kv", "Komi" },
    { "kw", "Cornish" },
    { "ky", "Kirghiz" },
    { "kok", "Konkani" },
    { "la", "Latin" },
    { "lb", "Letzeburgesch" },
    { "ln", "Lingala" },
    { "lo", "Laotian" },
    { "lt", "Lithuanian" },
    { "lv", "Latvian" },
    { "mg", "Malagasy" },
    { "mh", "Marshall" },
    { "mi", "Maori" },
    { "mk", "Macedonian" },
    { "ml", "Malayalam" },
    { "mn", "Mongolian" },
    { "mo", "Moldavian" },
    { "mr", "Marathi" },
    { "ms", "Malay" },
    { "mt", "Maltese" },
    { "my", "Burmese" },
    { "mni", "Manipuri" },
    { "na", "Nauru" },
    { "nb", "Norwegian Bokmal" },
    { "nd", "North Ndebele" },
    { "ne", "Nepali" },
    { "ng", "Ndonga" },
    { "nl", "Dutch" },
    { "nn", "Norwegian Nynorsk" },
    { "no", "Norwegian" },
    { "nr", "South Ndebele" },
    { "nv", "Navajo" },
    { "ny", "Nyanja" },
    { "oc", "Occitan" },
    { "om", "(Afan) Oromo" },
    { "or", "Oriya" },
    { "os", "Ossetian" },
    { "pa", "Punjabi" },
    { "pi", "Pali" },
    { "pl", "Polish" },
    { "ps", "Pashto" },
    { "pt", "Portuguese" },
    { "qu", "Quechua" },
    { "rm", "Rhaeto-Roman" },
    { "rn", "Kirundi" },
    { "ro", "Romanian" },
    { "ru", "Russian" },
    { "rw", "Kinyarwanda" },
    { "sa", "Sanskrit" },
    { "sc", "Sardinian" },
    { "sd", "Sindhi" },
    { "se", "Northern Sami" },
    { "sg", "Sango" },
    { "si", "Sinhalese" },
    { "sk", "Slovak" },
    { "sl", "Slovenian" },
    { "sm", "Samoan" },
    { "sn", "Shona" },
    { "so", "Somali" },
    { "sq", "Albanian" },
    { "sr", "Serbian" },
    { "ss", "Siswati" },
    { "st", "Sesotho" },
    { "su", "Sundanese" },
    { "sv", "Swedish" },
    { "sw", "Swahili" },
    { "ta", "Tamil" },
    { "te", "Telugu" },
    { "tg", "Tajik" },
    { "th", "Thai" },
    { "ti", "Tigrinya" },
    { "tk", "Turkmen" },
    { "tl", "Tagalog" },
    { "tn", "Setswana" },
    { "to", "Tonga" },
    { "tr", "Turkish" },
    { "ts", "Tsonga" },
    { "tt", "Tatar" },
    { "tw", "Twi" },
    { "ty", "Tahitian" },
    { "ug", "Uighur" },
    { "uk", "Ukrainian" },
    { "ur", "Urdu" },
    { "uz", "Uzbek" },
    { "vi", "Vietnamese" },
    { "vo", "Volapuk" },
    { "wo", "Wolof" },
    { "wen", "Sorbian" },
    { "xh", "Xhosa" },
    { "yi", "Yiddish" },
    { "yo", "Yoruba" },
    { "za", "Zhuang" },
    { "zh", "Chinese" },
    { "zu", "Zulu" }
  };
  size_t i;

  for (i = 0; i < SIZEOF (table); i ++)
    if (strcmp (table[i].code, language) == 0)
      return table[i].english;

  return xasprintf ("Language %s", language);
}


/* Construct the value for the PACKAGE name.  */
static const char *
project_id ()
{
  char *gettextlibdir;
  char *prog;
  char *argv[3];
  pid_t child;
  int fd[1];
  FILE *fp;
  char *line;
  size_t linesize;
  size_t linelen;
  int exitstatus;

  gettextlibdir = getenv ("GETTEXTLIBDIR");
  if (gettextlibdir == NULL || gettextlibdir[0] == '\0')
    gettextlibdir = concatenated_pathname (LIBDIR, "gettext", NULL);

  prog = concatenated_pathname (gettextlibdir, "project-id", NULL);

  /* Call the project-id shell script.  */
  argv[0] = "/bin/sh";
  argv[1] = prog;
  argv[2] = NULL;
  child = create_pipe_in (prog, "/bin/sh", argv, "/dev/null", false, true, fd);

  /* Retrieve its result.  */
  fp = fdopen (fd[0], "r");
  if (fp == NULL)
    error (EXIT_FAILURE, errno, _("fdopen() failed"));

  line = NULL; linesize = 0;
  linelen = getline (&line, &linesize, fp);
  if (linelen == (size_t)(-1))
    error (EXIT_FAILURE, 0, _("%s subprocess I/O error"), prog);
  if (linelen > 0 && line[linelen - 1] == '\n')
    line[linelen - 1] = '\0';

  fclose (fp);

  /* Remove zombie process from process list, and retrieve exit status.  */
  exitstatus = wait_subprocess (child, prog, true);
  if (exitstatus != 0)
    error (EXIT_FAILURE, 0, _("%s subprocess failed with exit code %d"),
	   prog, exitstatus);

  return line;
}


/* Construct the value for the Project-Id-Version field.  */
static const char *
project_id_version ()
{
  char *gettextlibdir;
  char *prog;
  char *argv[4];
  pid_t child;
  int fd[1];
  FILE *fp;
  char *line;
  size_t linesize;
  size_t linelen;
  int exitstatus;

  gettextlibdir = getenv ("GETTEXTLIBDIR");
  if (gettextlibdir == NULL || gettextlibdir[0] == '\0')
    gettextlibdir = concatenated_pathname (LIBDIR, "gettext", NULL);

  prog = concatenated_pathname (gettextlibdir, "project-id", NULL);

  /* Call the project-id shell script.  */
  argv[0] = "/bin/sh";
  argv[1] = prog;
  argv[2] = "yes";
  argv[3] = NULL;
  child = create_pipe_in (prog, "/bin/sh", argv, "/dev/null", false, true, fd);

  /* Retrieve its result.  */
  fp = fdopen (fd[0], "r");
  if (fp == NULL)
    error (EXIT_FAILURE, errno, _("fdopen() failed"));

  line = NULL; linesize = 0;
  linelen = getline (&line, &linesize, fp);
  if (linelen == (size_t)(-1))
    error (EXIT_FAILURE, 0, _("%s subprocess I/O error"), prog);
  if (linelen > 0 && line[linelen - 1] == '\n')
    line[linelen - 1] = '\0';

  fclose (fp);

  /* Remove zombie process from process list, and retrieve exit status.  */
  exitstatus = wait_subprocess (child, prog, true);
  if (exitstatus != 0)
    error (EXIT_FAILURE, 0, _("%s subprocess failed with exit code %d"),
	   prog, exitstatus);

  return line;
}


/* Construct the value for the PO-Revision-Date field.  */
static const char *
po_revision_date (header)
     const char *header;
{
  if (no_translator)
    /* Because the PO file is automatically generated, we use the
       POT-Creation-Date, not the current time.  */
    return get_field (header, "POT-Creation-Date");
  else
    {
      /* Assume the translator will modify the PO file now.  */
      time_t now;

      time (&now);
      return po_strftime (&now);
    }
}


/* Returns the struct passwd entry for the current user.  */
static struct passwd *
get_user_pwd ()
{
  const char *username;
  struct passwd *userpasswd;

  /* 1. attempt: getpwnam(getenv("USER"))  */
  username = getenv ("USER");
  if (username != NULL)
    {
      errno = 0;
      userpasswd = getpwnam (username);
      if (userpasswd != NULL)
	return userpasswd;
      if (errno != 0)
	error (EXIT_FAILURE, errno, "getpwnam(\"%s\")", username);
    }

  /* 2. attempt: getpwnam(getlogin())  */
  username = getlogin ();
  if (username != NULL)
    {
      errno = 0;
      userpasswd = getpwnam (username);
      if (userpasswd != NULL)
	return userpasswd;
      if (errno != 0)
	error (EXIT_FAILURE, errno, "getpwnam(\"%s\")", username);
    }

  /* 3. attempt: getpwuid(getuid())  */
  errno = 0;
  userpasswd = getpwuid (getuid ());
  if (userpasswd != NULL)
    return userpasswd;
  if (errno != 0)
    error (EXIT_FAILURE, errno, "getpwuid(\"%d\")", getuid ());

  return NULL;
}


/* Return the user's full name.  */
static const char *
get_user_fullname ()
{
  struct passwd *pwd;
  const char *fullname;
  const char *fullname_end;
  char *result;

  pwd = get_user_pwd ();
  if (pwd == NULL)
    return NULL;

  /* Return the pw_gecos field, upto the first comma (if any).  */
  fullname = pwd->pw_gecos;
  fullname_end = strchr (fullname, ',');
  if (fullname_end == NULL)
    fullname_end = fullname + strlen (fullname);

  result = (char *) xmalloc (fullname_end - fullname + 1);
  memcpy (result, fullname, fullname_end - fullname);
  result[fullname_end - fullname] = '\0';

  return result;
}


/* Return the user's email address.  */
static const char *
get_user_email ()
{
  char *prog = concatenated_pathname (LIBDIR, "gettext/user-email", NULL);
  char *argv[4];
  pid_t child;
  int fd[1];
  FILE *fp;
  char *line;
  size_t linesize;
  size_t linelen;
  int exitstatus;

  /* Ask the user for his email address.  */
  argv[0] = "/bin/sh";
  argv[1] = prog;
  argv[2] = _("\
The new message catalog should contain your email address, so that users can\n\
give you feedback about the translations, and so that maintainers can contact\n\
you in case of unexpected technical problems.\n");
  argv[3] = NULL;
  child = create_pipe_in (prog, "/bin/sh", argv, "/dev/null", false, true, fd);

  /* Retrieve his answer.  */
  fp = fdopen (fd[0], "r");
  if (fp == NULL)
    error (EXIT_FAILURE, errno, _("fdopen() failed"));

  line = NULL; linesize = 0;
  linelen = getline (&line, &linesize, fp);
  if (linelen == (size_t)(-1))
    error (EXIT_FAILURE, 0, _("%s subprocess I/O error"), prog);
  if (linelen > 0 && line[linelen - 1] == '\n')
    line[linelen - 1] = '\0';

  fclose (fp);

  /* Remove zombie process from process list, and retrieve exit status.  */
  exitstatus = wait_subprocess (child, prog, true);
  if (exitstatus != 0)
    error (EXIT_FAILURE, 0, _("%s subprocess failed with exit code %d"),
	   prog, exitstatus);

  return line;
}


/* Construct the value for the Last-Translator field.  */
static const char *
last_translator ()
{
  if (no_translator)
    return "Automatically generated";
  else
    {
      const char *fullname = get_user_fullname ();
      const char *email = get_user_email ();

      if (fullname != NULL)
	return xasprintf ("%s <%s>", fullname, email);
      else
	return xasprintf ("<%s>", email);
    }
}


/* Return the language team's mailing list address or homepage URL.  */
static const char *
language_team_address ()
{
  char *prog = concatenated_pathname (PROJECTSDIR, "team-address", NULL);
  char *argv[7];
  pid_t child;
  int fd[1];
  FILE *fp;
  char *line;
  size_t linesize;
  size_t linelen;
  int exitstatus;

  /* Call the team-address shell script.  */
  argv[0] = "/bin/sh";
  argv[1] = prog;
  argv[2] = PROJECTSDIR;
  argv[3] = concatenated_pathname (LIBDIR, "gettext", NULL);
  argv[4] = (char *) catalogname;
  argv[5] = (char *) language;
  argv[6] = NULL;
  child = create_pipe_in (prog, "/bin/sh", argv, "/dev/null", false, true, fd);

  /* Retrieve its result.  */
  fp = fdopen (fd[0], "r");
  if (fp == NULL)
    error (EXIT_FAILURE, errno, _("fdopen() failed"));

  line = NULL; linesize = 0;
  linelen = getline (&line, &linesize, fp);
  if (linelen == (size_t)(-1))
    line = "";
  else if (linelen > 0 && line[linelen - 1] == '\n')
    line[linelen - 1] = '\0';

  fclose (fp);

  /* Remove zombie process from process list, and retrieve exit status.  */
  exitstatus = wait_subprocess (child, prog, true);
  if (exitstatus != 0)
    error (EXIT_FAILURE, 0, _("%s subprocess failed with exit code %d"),
	   prog, exitstatus);

  return line;
}


/* Construct the value for the Language-Team field.  */
static const char *
language_team ()
{
  if (no_translator)
    return "none";
  else
    {
      const char *englishname = englishname_of_language ();
      const char *address = language_team_address ();

      if (address != NULL && address[0] != '\0')
	return xasprintf ("%s %s", englishname, address);
      else
	return englishname;
    }
}


/* Construct the value for the MIME-Version field.  */
static const char *
mime_version ()
{
  return "1.0";
}


/* Construct the value for the Content-Type field.  */
static const char *
content_type ()
{
  return xasprintf ("text/plain; charset=%s", canonical_locale_charset ());
}


/* Construct the value for the Content-Transfer-Encoding field.  */
static const char *
content_transfer_encoding ()
{
  return "8bit";
}


/* Construct the value for the Plural-Forms field.  */
static const char *
plural_forms ()
{
  size_t i;

  /* Search for a formula depending on the catalogname.  */
  for (i = 0; i < plural_table_size; i++)
    if (strcmp (plural_table[i].lang, catalogname) == 0)
      return plural_table[i].value;

  /* Search for a formula depending on the language only.  */
  for (i = 0; i < plural_table_size; i++)
    if (strcmp (plural_table[i].lang, language) == 0)
      return plural_table[i].value;

  return NULL;
}


static struct
{
  const char *name;
  const char * (*getter0) PARAMS ((void));
  const char * (*getter1) PARAMS ((const char *header));
}
fields[] =
  {
    { "Project-Id-Version", project_id_version, NULL },
    { "PO-Revision-Date", NULL, po_revision_date },
    { "Last-Translator", last_translator, NULL },
    { "Language-Team", language_team, NULL },
    { "MIME-Version", mime_version, NULL },
    { "Content-Type", content_type, NULL },
    { "Content-Transfer-Encoding", content_transfer_encoding, NULL },
    { "Plural-Forms", plural_forms, NULL }
  };

#define NFIELDS SIZEOF (fields)
#define FIELD_LAST_TRANSLATOR 2


/* Retrieve a freshly allocated copy of a field's value.  */
static char *
get_field (header, field)
     const char *header;
     const char *field;
{
  size_t len = strlen (field);
  const char *line;

  for (line = header;;)
    {
      if (strncmp (line, field, len) == 0
	  && line[len] == ':' && line[len + 1] == ' ')
	{
	  const char *value_start;
	  const char *value_end;
	  char *value;

	  value_start = line + len + 2;
	  value_end = strchr (value_start, '\n');
	  if (value_end == NULL)
	    value_end = value_start + strlen (value_start);

	  value = (char *) xmalloc (value_end - value_start + 1);
	  memcpy (value, value_start, value_end - value_start);
	  value[value_end - value_start] = '\0';

	  return value;
	}

      line = strchr (line, '\n');
      if (line != NULL)
	line++;
      else
	break;
    }

  return NULL;
}

/* Add a field with value to a header, and return the new header.  */
static char *
put_field (old_header, field, value)
     const char *old_header;
     const char *field;
     const char *value;
{
  size_t len = strlen (field);
  const char *line;
  char *new_header;
  char *p;

  for (line = old_header;;)
    {
      if (strncmp (line, field, len) == 0
	  && line[len] == ':' && line[len + 1] == ' ')
	{
	  const char *value_start;
	  const char *value_end;

	  value_start = line + len + 2;
	  value_end = strchr (value_start, '\n');
	  if (value_end == NULL)
	    value_end = value_start + strlen (value_start);

	  new_header = (char *) xmalloc (strlen (old_header)
					 - (value_end - value_start)
					 + strlen (value)
					 + (*value_end != '\n' ? 1 : 0)
					 + 1);
	  p = new_header;
	  memcpy (p, old_header, value_start - old_header);
	  p += value_start - old_header;
	  memcpy (p, value, strlen (value));
	  p += strlen (value);
	  if (*value_end != '\n')
	    *p++ = '\n';
	  strcpy (p, value_end);

	  return new_header;
	}

      line = strchr (line, '\n');
      if (line != NULL)
	line++;
      else
	break;
    }

  new_header = (char *) xmalloc (strlen (old_header) + 1
				 + len + 2 + strlen (value) + 1
				 + 1);
  p = new_header;
  memcpy (p, old_header, strlen (old_header));
  p += strlen (old_header);
  if (p > new_header && p[-1] != '\n')
    *p++ = '\n';
  memcpy (p, field, len);
  p += len;
  *p++ = ':';
  *p++ = ' ';
  memcpy (p, value, strlen (value));
  p += strlen (value);
  *p++ = '\n';
  *p = '\0';

  return new_header;
}


/* Return the title format string.  */
static const char *
get_title ()
{
  /* This is tricky.  We want the translation in the given locale specified by
     the command line, not the current locale.  But we want it in the encoding
     that we put into the header entry, not the encoding of that locale.
     We could avoid the use of OUTPUT_CHARSET by using a separate message
     catalog and bind_textdomain_codeset(), but that doesn't seem worth the
     trouble for one single message.  */
  const char *encoding;
  const char *tmp;
  char *old_LC_ALL;
  char *old_LANGUAGE;
  char *old_OUTPUT_CHARSET;
  const char *msgid;
  const char *english;
  const char *result;

  encoding = canonical_locale_charset ();

  /* First, the English title.  */
  english = xasprintf ("%s translations for %%s package",
		       englishname_of_language ());

  /* Save LC_ALL, LANGUAGE, OUTPUT_CHARSET environment variables.  */

  tmp = getenv ("LC_ALL");
  old_LC_ALL = (tmp != NULL ? xstrdup (tmp) : NULL);

  tmp = getenv ("LANGUAGE");
  old_LANGUAGE = (tmp != NULL ? xstrdup (tmp) : NULL);

  tmp = getenv ("OUTPUT_CHARSET");
  old_OUTPUT_CHARSET = (tmp != NULL ? xstrdup (tmp) : NULL);

  xsetenv ("LC_ALL", locale, 1);
  unsetenv ("LANGUAGE");
  xsetenv ("OUTPUT_CHARSET", encoding, 1);

#ifdef HAVE_SETLOCALE
  if (setlocale (LC_ALL, "") == NULL)
    /* Nonexistent locale.  Use the English title.  */
    result = english;
  else
#endif
    {
      /* Fetch the translation.  */
      /* TRANSLATORS: "English" needs to be replaced by your language.
	 For example in it.po write "Traduzioni italiani ...",
	 *not* "Traduzioni inglesi ...".  */
      msgid = N_("English translations for %s package");
      result = gettext (msgid);
      if (result != msgid && strcmp (result, msgid) != 0)
	/* Use the English and the foreign title.  */
	result = xasprintf ("%s\n%s", english, result);
      else
	/* No translation found.  Use the English title.  */
	result = english;
    }

  /* Restore LC_ALL, LANGUAGE, OUTPUT_CHARSET environment variables.  */

  if (old_LC_ALL != NULL)
    xsetenv ("LC_ALL", old_LC_ALL, 1), free (old_LC_ALL);
  else
    unsetenv ("LC_ALL");

  if (old_LANGUAGE != NULL)
    xsetenv ("LANGUAGE", old_LANGUAGE, 1), free (old_LANGUAGE);
  else
    unsetenv ("LANGUAGE");

  if (old_OUTPUT_CHARSET != NULL)
    xsetenv ("OUTPUT_CHARSET", old_OUTPUT_CHARSET, 1), free (old_OUTPUT_CHARSET);
  else
    unsetenv ("OUTPUT_CHARSET");

#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif

  return result;
}


/* Perform a set of substitutions in a string and return the resulting
   string.  When subst[j][0] found, it is replaced with subst[j][1].
   subst[j][0] must not be the empty string.  */
static const char *
subst_string (str, nsubst, subst)
     const char *str;
     unsigned int nsubst;
     const char *(*subst)[2];
{
  if (nsubst > 0)
    {
      char *malloced = NULL;
      size_t *substlen;
      size_t i;
      unsigned int j;

      substlen = (size_t *) alloca (nsubst * sizeof (size_t));
      for (j = 0; j < nsubst; j++)
	{
	  substlen[j] = strlen (subst[j][0]);
	  if (substlen[j] == 0)
	    abort ();
	}

      for (i = 0;;)
	{
	  if (str[i] == '\0')
	    break;
	  for (j = 0; j < nsubst; j++)
	    if (*(str + i) == *subst[j][0]
		&& strncmp (str + i, subst[j][0], substlen[j]) == 0)
	      {
		size_t replacement_len = strlen (subst[j][1]);
		size_t new_len = strlen (str) - substlen[j] + replacement_len;
		char *new_str = (char *) xmalloc (new_len + 1);
		memcpy (new_str, str, i);
		memcpy (new_str + i, subst[j][1], replacement_len);
		strcpy (new_str + i + replacement_len, str + i + substlen[j]);
		if (malloced != NULL)
		  free (malloced);
		str = new_str;
		malloced = new_str;
		i += replacement_len;
		break;
	      }
	  if (j == nsubst)
	    i++;
	}
    }

  return str;
}

/* Perform a set of substitutions on each string of a string list.
   When subst[j][0] found, it is replaced with subst[j][1].  subst[j][0]
   must not be the empty string.  */
static void
subst_string_list (slp, nsubst, subst)
     string_list_ty *slp;
     unsigned int nsubst;
     const char *(*subst)[2];
{
  size_t j;

  for (j = 0; j < slp->nitems; j++)
    slp->item[j] = subst_string (slp->item[j], nsubst, subst);
}


/* Fill the templates in all fields of the header entry.  */
static msgdomain_list_ty *
fill_header (mdlp)
     msgdomain_list_ty *mdlp;
{
  /* Cache the strings filled in, for use when there are multiple domains
     and a header entry for each domain.  */
  const char *field_value[NFIELDS];
  size_t k, j, i;

  for (i = 0; i < NFIELDS; i++)
    field_value[i] = NULL;

  for (k = 0; k < mdlp->nitems; k++)
    {
      message_list_ty *mlp = mdlp->item[k]->messages;

      if (mlp->nitems > 0)
	{
	  message_ty *header_mp = NULL;
	  char *header;
	  const char *subst[4][2];
	  const char *id;
	  time_t now;

	  /* Search the header entry.  */
	  for (j = 0; j < mlp->nitems; j++)
	    if (mlp->item[j]->msgid[0] == '\0' && !mlp->item[j]->obsolete)
	      {
		header_mp = mlp->item[j];
		break;
	      }

	  /* If it wasn't found, provide one.  */
	  if (header_mp == NULL)
	    {
	      static lex_pos_ty pos = { __FILE__, __LINE__ };

	      header_mp = message_alloc ("", NULL, "", 1, &pos);
	      message_list_prepend (mlp, header_mp);
	    }

	  header = xstrdup (header_mp->msgstr);

	  /* Fill in the fields.  */
	  for (i = 0; i < NFIELDS; i++)
	    {
	      if (field_value[i] == NULL)
		field_value[i] =
		  (fields[i].getter1 != NULL
		   ? fields[i].getter1 (header)
		   : fields[i].getter0 ());

	      if (field_value[i] != NULL)
		{
		  char *old_header = header;
		  header = put_field (header, fields[i].name, field_value[i]);
		  free (old_header);
		}
	    }

	  /* Replace the old translation in the header entry.  */
	  header_mp->msgstr = header;
	  header_mp->msgstr_len = strlen (header) + 1;

	  /* Update the comments in the header entry.  */
	  id = project_id ();
	  subst[0][0] = "SOME DESCRIPTIVE TITLE";
	  subst[0][1] = xasprintf (get_title (), id, id);
	  subst[1][0] = "PACKAGE";
	  subst[1][1] = id;
	  subst[2][0] = "FIRST AUTHOR <EMAIL@ADDRESS>";
	  subst[2][1] = field_value[FIELD_LAST_TRANSLATOR];
	  subst[3][0] = "YEAR";
	  subst[3][1] =
	    xasprintf ("%d", (time (&now), (localtime (&now))->tm_year + 1900));
	  subst_string_list (header_mp->comment, SIZEOF (subst), subst);

	  /* Finally remove the fuzzy attribute.  */
	  header_mp->is_fuzzy = false;
	}
    }

  return mdlp;
}
