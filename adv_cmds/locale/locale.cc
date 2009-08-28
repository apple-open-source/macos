#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

#include <unistd.h>
#include <langinfo.h>
#include <locale.h>
#include <xlocale.h>
#include <sys/types.h>
#include <dirent.h>

#define LAST(array) array + (sizeof(array) / sizeof(*array))

#define LC_SPECIAL (LC_COLLATE+LC_CTYPE+LC_MESSAGES+LC_MONETARY+LC_NUMERIC+LC_TIME)

using namespace std;

enum vtype {
	V_STR, V_NUM
};

template<typename T> string tostr(T val) {
	ostringstream ss;
	ss << val;
	return ss.str();
}

string quote(string s) {
	if (s.length() == 0) {
		return "";
	}

	return '"' + s + '"';
}

class keyword {
  public:
	virtual string get_category() const { return category; }
	virtual string get_keyword() const { return kword; }
	virtual string get_value(bool show_quotes) const {
	  return (show_quotes && t == V_STR) ? quote(value) : value; }
	
	virtual ~keyword() { }
  protected:
	keyword(int category_, string kword, string value, vtype t)
	  : kword(kword), value(value), t(t) {
		switch(category_) {
			case LC_COLLATE:
				category = "LC_COLLATE";
				break;
			case LC_CTYPE:
				category = "LC_CTYPE";
				break;
			case LC_MESSAGES:
				category = "LC_MESSAGES";
				break;
			case LC_MONETARY:
				category = "LC_MONETARY";
				break;
			case LC_NUMERIC:
				category = "LC_NUMERIC";
				break;
			case LC_TIME:
				category = "LC_TIME";
				break;
			case LC_SPECIAL:
				category = "LC_SPECIAL";
				break;
			default:
				{
					ostringstream lc;
					lc << "LC_" << category_;
					category = lc.str();
				}
				break;
		}
	}

	string category, kword, value;
	vtype t;
};

struct keyword_cmp {
	bool operator()(const keyword *a, const keyword *b) const {
		return a->get_category() < b->get_category();
	}
};

class li_keyword : public keyword {
  public:
	li_keyword(int category, string kword, int itemnum, vtype t = V_STR)
	  : keyword(category, kword, nl_langinfo(itemnum), t) { }
};

class lia_keyword : public keyword {
  protected:
	vector<string> values;
  public:
	virtual string get_value(bool show_quotes) const {
		ostringstream ss;
		vector<string>::const_iterator s(values.begin()), e(values.end()), i(s);

		for(; i < e; ++i) {
			if (i != s) {
				ss << ';';
			}
			if (show_quotes && t == V_STR) {
				ss << quote(*i);
			} else {
				ss << *i;
			}
		}

		return ss.str();
	}

	lia_keyword(int category, string kword, int *s, int *e, vtype t = V_STR)
	  : keyword(category, kword, "", t) {
		for(; s < e; ++s) {
			values.push_back(nl_langinfo(*s));
		}
	}
};

class lc_keyword : public keyword {
  public:
	lc_keyword(int category, string kword, string value, vtype t = V_STR)
	  : keyword(category, kword, value, t) { }
};

void usage(char *argv0) {
	clog << "usage: " << argv0 << "[-a|-m]\n   or: "
	  << argv0 << " [-cCk] name..." << endl;
}

void list_all_valid_locales() {
	string locale_dir("/usr/share/locale");
	bool found_C = false, found_POSIX = false;
	DIR *d = opendir(locale_dir.c_str());
	struct dirent *de;
	static string expected[] = { "LC_COLLATE", "LC_CTYPE", "LC_MESSAGES",
	  "LC_NUMERIC", "LC_TIME" };

	for(de = readdir(d); de; de = readdir(d)) {
		string lname(de->d_name, de->d_namlen);
		string ldir(locale_dir + "/" + lname);
		int cnt = 0;
		DIR *ld = opendir(ldir.c_str());
		if (ld) {
			struct dirent *lde;
			for(lde = readdir(ld); lde; lde = readdir(ld)) {
				string fname(lde->d_name, lde->d_namlen);
				if (LAST(expected) != find(expected, LAST(expected), fname)) {
					cnt++;
				}
			}
			closedir(ld);

			if (cnt == LAST(expected) - expected) {
				cout << lname << endl;
				if (lname == "C") {
					found_C = true;
				}
				if (lname == "POSIX") {
					found_POSIX = true;
				}
			}
		}
	}
	closedir(d);
	if (!found_C) {
		cout << "C" << endl;
	}
	if (!found_POSIX) {
		cout << "POSIX" << endl;
	}
}

void show_all_unique_codesets() {
	string locale_dir("/usr/share/locale");
	DIR *d = opendir(locale_dir.c_str());
	struct dirent *de;
	static string expected[] = { "LC_COLLATE", "LC_CTYPE", "LC_MESSAGES",
	  "LC_NUMERIC", "LC_TIME" };
	set<string> codesets;
	for(de = readdir(d); de; de = readdir(d)) {
		string lname(de->d_name, de->d_namlen);
		string ldir(locale_dir + "/" + lname);
		int cnt = 0;
		DIR *ld = opendir(ldir.c_str());
		if (ld) {
			struct dirent *lde;
			for(lde = readdir(ld); lde; lde = readdir(ld)) {
				string fname(lde->d_name, lde->d_namlen);
				if (LAST(expected) != find(expected, LAST(expected), fname)) {
					cnt++;
				}
			}
			closedir(ld);

			if (cnt == LAST(expected) - expected) {
				locale_t xloc = newlocale(LC_ALL_MASK, lname.c_str(), NULL);
				if (xloc) {
					char *cs = nl_langinfo_l(CODESET, xloc);
					if (cs && *cs && (codesets.find(cs) == codesets.end())) {
						cout << cs << endl;
						codesets.insert(cs);
					}
					freelocale(xloc);
				}
			}
		}
	}
	closedir(d);
}

typedef map<string, keyword *> keywords_t;
keywords_t keywords;

typedef map<string, vector<keyword *> > catorgies_t;
catorgies_t catoriges;

void add_kw(keyword *k) {
	keywords.insert(make_pair(k->get_keyword(), k));
	catorgies_t::iterator c = catoriges.find(k->get_category());
	if (c != catoriges.end()) {
		c->second.push_back(k);
	} else {
		vector<keyword *> v;
		v.push_back(k);
		catoriges.insert(make_pair(k->get_category(), v));
	}
}

string grouping(char *g) {
	ostringstream ss;
	if (*g == 0) {
	    ss << "0";
	} else {
	    ss << static_cast<int>(*g);
	    while(*++g) {
		ss << ";" << static_cast<int>(*g);
	    }
	}
	return ss.str();
}

void init_keywords() {
	struct lconv *lc = localeconv();
	if (lc) {
		add_kw(new lc_keyword(LC_NUMERIC, "decimal_point", lc->decimal_point));
		add_kw(new lc_keyword(LC_NUMERIC, "thousands_sep", lc->thousands_sep));
		add_kw(new lc_keyword(LC_NUMERIC, "grouping", grouping(lc->grouping)));
		add_kw(new lc_keyword(LC_MONETARY, "int_curr_symbol", lc->int_curr_symbol));
		add_kw(new lc_keyword(LC_MONETARY, "currency_symbol", lc->currency_symbol));
		add_kw(new lc_keyword(LC_MONETARY, "mon_decimal_point", lc->mon_decimal_point));
		add_kw(new lc_keyword(LC_MONETARY, "mon_thousands_sep", lc->mon_thousands_sep));
		add_kw(new lc_keyword(LC_MONETARY, "mon_grouping", grouping(lc->mon_grouping)));
		add_kw(new lc_keyword(LC_MONETARY, "positive_sign", lc->positive_sign));
		add_kw(new lc_keyword(LC_MONETARY, "negative_sign", lc->negative_sign));
		add_kw(new lc_keyword(LC_MONETARY, "int_frac_digits", tostr((int)lc->int_frac_digits), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "frac_digits", tostr((int)lc->frac_digits), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "p_cs_precedes", tostr((int)lc->p_cs_precedes), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "p_sep_by_space", tostr((int)lc->p_sep_by_space), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "n_cs_precedes", tostr((int)lc->n_cs_precedes), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "n_sep_by_space", tostr((int)lc->n_sep_by_space), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "p_sign_posn", tostr((int)lc->p_sign_posn), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "n_sign_posn", tostr((int)lc->n_sign_posn), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "int_p_cs_precedes", tostr((int)lc->int_p_cs_precedes), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "int_n_cs_precedes", tostr((int)lc->int_n_cs_precedes), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "int_p_sep_by_space", tostr((int)lc->int_p_sep_by_space), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "int_n_sep_by_space", tostr((int)lc->int_n_sep_by_space), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "int_p_sign_posn", tostr((int)lc->int_p_sign_posn), V_NUM));
		add_kw(new lc_keyword(LC_MONETARY, "int_n_sign_posn", tostr((int)lc->int_n_sign_posn), V_NUM));
	}

	int abdays[] = {ABDAY_1, ABDAY_2, ABDAY_3, ABDAY_4, ABDAY_5, ABDAY_6, ABDAY_7};
	add_kw(new lia_keyword(LC_TIME, "ab_day", abdays, LAST(abdays)));
	add_kw(new lia_keyword(LC_TIME, "abday", abdays, LAST(abdays)));

	int days[] = {DAY_1, DAY_2, DAY_3, DAY_4, DAY_5, DAY_6, DAY_7};
	add_kw(new lia_keyword(LC_TIME, "day", days, LAST(days)));

	int abmons[] = {ABMON_1, ABMON_2, ABMON_3, ABMON_4, ABMON_5, ABMON_6, ABMON_7, ABMON_8, ABMON_9, ABMON_10, ABMON_11, ABMON_12};
	add_kw(new lia_keyword(LC_TIME, "abmon", abmons, LAST(abmons)));

	int mons[] = {MON_1, MON_2, MON_3, MON_4, MON_5, MON_6, MON_7, MON_8, MON_9, MON_10, MON_11, MON_12};
	add_kw(new lia_keyword(LC_TIME, "mon", mons, LAST(mons)));

	int am_pms[] = {AM_STR, PM_STR};
	add_kw(new lia_keyword(LC_TIME, "am_pm", am_pms, LAST(am_pms)));

	add_kw(new li_keyword(LC_TIME, "t_fmt_ampm", T_FMT_AMPM));
	add_kw(new li_keyword(LC_TIME, "era", ERA));
	add_kw(new li_keyword(LC_TIME, "era_d_fmt", ERA_D_FMT));
	add_kw(new li_keyword(LC_TIME, "era_t_fmt", ERA_T_FMT));
	add_kw(new li_keyword(LC_TIME, "era_d_t_fmt", ERA_D_T_FMT));
	add_kw(new li_keyword(LC_TIME, "alt_digits", ALT_DIGITS));

	add_kw(new li_keyword(LC_TIME, "d_t_fmt", D_T_FMT));
	add_kw(new li_keyword(LC_TIME, "d_fmt", D_FMT));
	add_kw(new li_keyword(LC_TIME, "t_fmt", T_FMT));

	add_kw(new li_keyword(LC_MESSAGES, "yesexpr", YESEXPR));
	add_kw(new li_keyword(LC_MESSAGES, "noexpr", NOEXPR));
	add_kw(new li_keyword(LC_MESSAGES, "yesstr", YESSTR));
	add_kw(new li_keyword(LC_MESSAGES, "nostr", NOSTR));

	add_kw(new li_keyword(LC_CTYPE, "charmap", CODESET));
	add_kw(new lc_keyword(LC_SPECIAL, "categories", "LC_COLLATE LC_CTYPE LC_MESSAGES LC_MONETARY LC_NUMERIC LC_TIME"));

	// add_kw: CRNCYSTR D_MD_ORDER CODESET RADIXCHAR THOUSEP
}

void show_keyword(string &last_cat, bool sw_categories, bool sw_keywords, 
  keyword *k) {
	if (sw_categories && last_cat != k->get_category()) {
		last_cat = k->get_category();
		cout << last_cat << endl;
	}
	if (sw_keywords) {
		cout << k->get_keyword() << "=";
	}
	cout << k->get_value(sw_keywords) << endl;
}

int main(int argc, char *argv[]) {
	int sw; 
	bool sw_all_locales = false, sw_categories = false, sw_keywords = false,
	  sw_charmaps = false;

	while(-1 != (sw = getopt(argc, argv, "ackm"))) {
		switch(sw) {
			case 'a':
				sw_all_locales = true;
				break;
			case 'c':
				sw_categories = true;
				break;
			case 'k':
				sw_keywords = true;
				break;
			case 'm':
				sw_charmaps = true;
				break;
			default:
				usage(argv[0]);
				exit(1);
		}
	}

	if ((sw_all_locales && sw_charmaps)
	  || ((sw_all_locales || sw_charmaps) && (sw_keywords || sw_categories))
	  ) {
		usage(argv[0]);
		exit(1);
	}

	setlocale(LC_ALL, "");

	if (!(sw_all_locales || sw_categories || sw_keywords || sw_charmaps)
	  && argc == optind) {
		char *lang = getenv("LANG");
		cout << "LANG=" << quote(lang ? lang : "") << endl;
		cout << "LC_COLLATE=" << quote(setlocale(LC_COLLATE, NULL)) << endl;
		cout << "LC_CTYPE=" << quote(setlocale(LC_CTYPE, NULL)) << endl;
		cout << "LC_MESSAGES=" << quote(setlocale(LC_MESSAGES, NULL)) << endl;
		cout << "LC_MONETARY=" << quote(setlocale(LC_MONETARY, NULL)) << endl;
		cout << "LC_NUMERIC=" << quote(setlocale(LC_NUMERIC, NULL)) << endl;
		cout << "LC_TIME=" << quote(setlocale(LC_TIME, NULL)) << endl;
		if (getenv("LC_ALL")) {
		    cout << "LC_ALL=" << quote(setlocale(LC_ALL, NULL)) << endl;
		} else {
		    cout << "LC_ALL=" << endl;
		}

		return 0;
	}

	if (sw_all_locales) {
		list_all_valid_locales();
		return 0;
	}

	if (sw_charmaps) {
	        show_all_unique_codesets();
		return 0;
	}

	init_keywords();
	string last_cat("");
	int exit_val = 0;
	for(int i = optind; i < argc; ++i) {
		keywords_t::iterator ki = keywords.find(argv[i]);
		if (ki != keywords.end()) {
			show_keyword(last_cat, sw_categories, sw_keywords, ki->second);
		} else {
			catorgies_t::iterator ci = catoriges.find(argv[i]);
			if (ci != catoriges.end()) {
				vector<keyword *>::iterator vi(ci->second.begin()),
				  ve(ci->second.end());
				for(; vi != ve; ++vi) {
					show_keyword(last_cat, sw_categories, sw_keywords, *vi);
				}
			} else if (argv[i] == string("LC_ALL")) {
			    ki = keywords.begin();
			    keywords_t::iterator ke = keywords.end();
			    for(; ki != ke; ++ki) {
				show_keyword(last_cat, sw_categories, sw_keywords, ki->second);
			    }
			} else {
				if (argv[i] == string("LC_CTYPE") 
				  || argv[i] == string("LC_COLLATE")) {
				    // It would be nice to print a warning,
				    // but we aren't allowed (locale.ex test#14)
				    if (sw_categories) {
					cout << argv[i] << endl;
				    }
				} else {
				    clog << "unknown keyword "
				      << argv[i] << endl;
				    exit_val = 1;
				}
			}
		}
	}

	return exit_val;
}
