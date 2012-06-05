/**
 * @file view_curses.hh
 */

#ifndef __view_curses_hh
#define __view_curses_hh

#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <limits.h>

#if defined HAVE_NCURSESW_CURSES_H
#  include <ncursesw/curses.h>
#elif defined HAVE_NCURSESW_H
#  include <ncursesw.h>
#elif defined HAVE_NCURSES_CURSES_H
#  include <ncurses/curses.h>
#elif defined HAVE_NCURSES_H
#  include <ncurses.h>
#elif defined HAVE_CURSES_H
#  include <curses.h>
#else
#  error "SysV or X/Open-compatible Curses header file required"
#endif

#include <map>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

class view_curses;

class screen_curses {
public:
    screen_curses()
	: sc_main_window(initscr()) {
	};
    virtual ~screen_curses() {
    	endwin();
    };

    WINDOW *get_window() { return this->sc_main_window; };

private:
    WINDOW *sc_main_window;
};

struct line_range {
    int lr_start;
    int lr_end;

    int length() const {
	return this->lr_end == -1 ? INT_MAX : this->lr_end - this->lr_start;
    };
    
    bool operator<(const struct line_range &rhs) const {
	if (this->lr_start < rhs.lr_start) return true;
	else if (this->lr_start > rhs.lr_start) return false;
	
	if (this->lr_end < rhs.lr_end) return true;
	return false;
    };
};

typedef union {
    void *sa_ptr;
    int sa_int;
} string_attr_t;

inline std::pair<std::string, string_attr_t>
make_string_attr(const std::string name, void *val)
{
    string_attr_t sa;

    sa.sa_ptr = val;

    return std::make_pair(name, sa);
}

inline std::pair<std::string, string_attr_t>
make_string_attr(const std::string name, int val)
{
    string_attr_t sa;
    
    sa.sa_int = val;

    return std::make_pair(name, sa);
}

typedef std::multimap<std::string, string_attr_t> attrs_map_t;
typedef std::map<struct line_range, attrs_map_t> string_attrs_t;

class attr_line_t {
public:
    attr_line_t() { };

    std::string &get_string() { return this->al_string; };
    
    string_attrs_t &get_attrs() { return this->al_attrs; };

    void operator=(const std::string &rhs) { this->al_string = rhs; };
    
    void clear() {
	this->al_string.clear();
	this->al_attrs.clear();
    };
    
private:
    std::string al_string;
    string_attrs_t al_attrs;
};

/**
 * Class that encapsulates a method to execute and the object on which to
 * execute it.
 *
 * @param _Sender The type of object that will be triggering an action.
 */
template<class _Sender>
class view_action {
public:

    /**
     *
     * @param _Receiver The type of object that will be triggered by an action.
     */
    template<class _Receiver>
    class mem_functor_t {
public:
	mem_functor_t(_Receiver &receiver,
		      void (_Receiver::*selector)(_Sender *))
	    : mf_receiver(receiver),
	      mf_selector(selector) { };

	void operator()(_Sender *sender)
	{
	    (this->mf_receiver.*mf_selector)(sender);
	};

	static void invoke(mem_functor_t *self, _Sender *sender)
	{
	    (*self)(sender);
	};

private:
	_Receiver & mf_receiver;
	void (_Receiver::*mf_selector)(_Sender *);
    };

    class broadcaster
	: public std::vector<view_action> {
public:

	broadcaster()
	    : b_functor(*this, &broadcaster::invoke) { };
	virtual ~broadcaster() { };

	void invoke(_Sender *sender)
	{
	    typename std::vector<view_action>::iterator iter;

	    for (iter = this->begin(); iter != this->end(); iter++) {
		(*iter).invoke(sender);
	    }
	};

	mem_functor_t<broadcaster> *get_functor()
	{
	    return &this->b_functor;
	};

private:
	mem_functor_t<broadcaster> b_functor;
    };

    /**
     * @param receiver The object to pass as the first argument to the selector
     * function.
     * @param selector The function to execute.  The function should take two
     * parameters, the first being the value of the receiver pointer and the
     * second being the sender pointer as passed to invoke().
     */
    view_action(void (*invoker)(void *, _Sender *) = NULL)
    	: va_functor(NULL),
    	  va_invoker(invoker) { };

    template<class _Receiver>
    view_action(mem_functor_t < _Receiver > *mf)
	: va_functor(mf),
	  va_invoker((void (*)(void *, _Sender *))
		     mem_functor_t<_Receiver>::invoke) { };

    /**
     * Performs a shallow copy of another view_action.
     *
     * @param va The view_action to copy the receiver and selector pointers
     * from.
     */
    view_action(const view_action &va)
	: va_functor(va.va_functor),
	  va_invoker(va.va_invoker) { };

    ~view_action() { };

    /**
     * @param rhs The view_action to shallow copy.
     * @return *this
     */
    view_action &operator=(const view_action &rhs)
    {
	this->va_functor = rhs.va_functor;
	this->va_invoker = rhs.va_invoker;

	return *this;
    };

    /**
     * Invoke the action by calling the selector function, if one is set.
     *
     * @param sender Pointer to the object that called this method.
     */
    void invoke(_Sender *sender)
    {
	if (this->va_invoker != NULL) {
	    this->va_invoker(this->va_functor, sender);
	}
    };

private:

    /** The object to pass as the first argument to the selector function.*/
    void *va_functor;
    /** The function to call when this action is invoke()'d. */
    void (*va_invoker)(void *functor, _Sender * sender);
};

/**
 * Singleton used to manage the colorspace.
 */
class view_colors {
public:

    /** Roles that can be mapped to curses attributes using attrs_for_role() */
    typedef enum {
	VCR_NONE = -1,

	VCR_TEXT,               /*< Raw text. */
	VCR_SEARCH,             /*< A search hit. */
	VCR_OK,
	VCR_ERROR,              /*< An error message. */
	VCR_WARNING,            /*< A warning message. */
	VCR_ALT_ROW,            /*< Highlight for alternating rows in a list */
	VCR_STATUS,             /*< Normal status line text. */
	VCR_WARN_STATUS,
	VCR_ALERT_STATUS,       /*< Alert status line text. */
	VCR_ACTIVE_STATUS,      /*< */
	VCR_ACTIVE_STATUS2,     /*< */

	VCR__MAX
    } role_t;

    /** @return A reference to the singleton. */
    static view_colors &singleton();

    /**
     * Performs curses-specific initialization.  The other methods can be
     * called before this method, but the returned attributes cannot be used
     * with curses code until this method is called.
     */
    void init(void);

    /**
     * @param role The role to retrieve character attributes for.
     * @return The attributes to use for the given role.
     */
    int attrs_for_role(role_t role)
    {
	assert(role >= 0);
	assert(role < VCR__MAX + (HL_COLOR_COUNT * 2));

	return this->vc_role_colors[role];
    };

    int reverse_attrs_for_role(role_t role)
    {
	assert(role >= 0);
	assert(role < VCR__MAX + (HL_COLOR_COUNT * 2));

	return this->vc_role_reverse_colors[role];
    };

    /**
     * @return The next set of attributes to use for highlighting text.  This
     * method will iterate through eight-or-so attributes combinations so there
     * is some variety in how text is highlighted.
     */
    role_t next_highlight(void);

    enum {
	VC_EMPTY = 0,       /* XXX Dead color pair, doesn't work. */

	VC_BLUE,
	VC_CYAN,
	VC_GREEN,
	VC_MAGENTA,

	VC_BLUE_ON_WHITE,
	VC_CYAN_ON_BLACK,
	VC_GREEN_ON_WHITE,
	VC_MAGENTA_ON_WHITE,

	VC_RED,
	VC_YELLOW,
	VC_WHITE,

	VC_BLACK_ON_WHITE,
	VC_YELLOW_ON_WHITE,
	VC_RED_ON_WHITE,

	VC_WHITE_ON_GREEN,
    };

private:

    /** The number of colors used for highlighting. */
    static const int HL_COLOR_COUNT = 4;

    /** Private constructor that initializes the member fields. */
    view_colors();

    /** Map of role IDs to attribute values. */
    int vc_role_colors[VCR__MAX + (HL_COLOR_COUNT * 2)];
    /** Map of role IDs to reverse-video attribute values. */
    int vc_role_reverse_colors[VCR__MAX + (HL_COLOR_COUNT * 2)];
    /** The index of the next highlight color to use. */
    int vc_next_highlight;
};

/**
 * Interface for "view" classes that will update a curses(3) display.
 */
class view_curses {
public:
    virtual ~view_curses() { };

    /**
     * Update the curses display.
     */
    virtual void do_update(void) = 0;

    void mvwattrline(WINDOW *window,
		     int y,
		     int x,
		     attr_line_t &al,
		     struct line_range &lr,
		     view_colors::role_t base_role = view_colors::VCR_TEXT);
};

#endif
