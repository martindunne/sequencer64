/*
 *  This file is part of seq24/sequencer64.
 *
 *  seq24 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  seq24 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with seq24; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file          midibase.cpp
 *
 *  This module declares/defines the base class for handling MIDI I/O via
 *  various MIDI APIs.
 *
 * \library       sequencer64 application
 * \author        Seq24 team; modifications by Chris Ahlstrom
 * \date          2016-11-25
 * \updates       2016-11-25
 * \license       GNU GPLv2 or above
 *
 *  This file provides a cross-platform implementation of MIDI support.
 */

#include "globals.h"
#include "calculations.hpp"             /* clock_ticks_from_ppqn()          */
#include "event.hpp"                    /* seq64::event (MIDI event)        */
#include "midibase.hpp"                 /* seq64::midibase for ALSA         */
#include "settings.hpp"                 /* seq64::rc() and choose_ppqn()    */

/*
 *  Do not document a namespace; it breaks Doxygen.
 */

namespace seq64
{

/**
 *  Initialize this static member.
 */

int midibase::m_clock_mod = 16 * 4;

/**
 *  Provides a constructor with client number, port number, ALSA sequencer
 *  support, name of client, name of port.
 *
 *  This constructor is the one that seems to be the one that is used for
 *  the MIDI input and output busses, when the [manual-alsa-ports] option is
 *  not in force.  Also used for the announce buss, and in the
 *  mastermidibase::port_start() function.
 *
 * \param localclient
 *      Provides the local-client number.
 *
 * \param destclient
 *      Provides the destination-client number.
 *
 * \param destport
 *      Provides the destination-client port.
 *
 * \param seq
 *      Provides the ALSA sequence that will work with this buss.
 *
 * \param client_name
 *      Provides the client name, but this parameter is unused.
 *
 * \param port_name
 *      Provides the port name.
 *
 * \param id
 *      Provides the ID code for this bus.  It is an index into the midibase
 *      definitions array, and is also used in the constructed human-readable
 *      buss name.
 *
 * \param queue
 *      Provides the queue ID.
 *
 * \param ppqn
 *      Provides the PPQN value.
 */

midibase::midibase
(
    const std::string & clientname,
    const std::string & portname,
    int id,
    int queue,
    int ppqn
) :
    m_id                (id),
    m_clock_type        (e_clock_off),
    m_inputing          (false),
    m_ppqn              (choose_ppqn(ppqn)),
    m_queue             (queue),
    m_bus_name          (clientname),
    m_port_name         (portname),
    m_lasttick          (0),
    m_mutex             ()
{
    // Some settings made in derived class.
}

/**
 *  A rote empty destructor.
 */

midibase::~midibase()
{
    // empty body
}

/**
 *  Polls for MIDI events.
 */

int
midibase::poll_for_midi ()
{
    return api_poll_for_midi();
}

/**
 *  Initialize the MIDI output port.
 *
 * \return
 *      Returns true unless setting up MIDI failed in some way.
 */

bool
midibase::init_out ()
{
    return api_init_out();
}

/**
 *  Initialize the MIDI input port.
 *
 * \return
 *      Returns true unless setting up MIDI failed in some way.
 */

bool
midibase::init_in ()
{
    return api_init_in();
}

/**
 *  Initialize the output in a different way?
 *
 * \return
 *      Returns true unless setting up the ALSA port failed in some way.
 */

bool
midibase::init_out_sub ()
{
    return api_init_out_sub();      // no portmidi implementation
}

/**
 *  Initialize the output in a different way?
 *
 * \return
 *      Returns true unless setting up the ALSA port failed in some way.
 */

bool
midibase::init_in_sub ()
{
    return api_init_in_sub();       // no portmidi implementation
}

/**
 *  Deinitialize the MIDI input.  Set the input and the output ports.
 *  The destination port is actually our local port.
 *
 * \return
 *      Returns true, unless an error occurs.
 */

bool
midibase::deinit_in ()
{
    return api_deinit_in();
}

/**
 *  Prints m_name.
 */

void
midibase::print ()
{
    printf("%s:%s", m_bus_name.c_str(), m_port_name.c_str());
}

/**
 *  This play() function takes a native event, encodes it to a MIDI
 *  sequencer event, sets the broadcasting to the subscribers, sets the
 *  direct-passing mode to send the event without queueing, and puts it in the
 *  queue.
 *
 * \threadsafe
 *
 * \param e24
 *      The event to be played on this bus.  For speed, we don't bother to
 *      check the pointer.
 *
 * \param channel
 *      The channel of the playback.
 */

void
midibase::play (event * e24, midibyte channel)
{
    automutex locker(m_mutex);
    api_play(e24, channel);
}

/**
 *  Takes a native SYSEX event, encodes it to an ALSA event, and then
 *  puts it in the queue.
 *
 * \param e24
 *      The event to be handled.
 */

void
midibase::sysex (event * e24)
{
    automutex locker(m_mutex);
    api_sysex(e24);
}

/**
 *  Flushes our local queue events out into ALSA.
 */

void
midibase::flush ()
{
    automutex locker(m_mutex);
    api_flush();
}

/**
 *  Initialize the clock, continuing from the given tick.  This function doesn't
 *  depend upon the MIDI API in use.
 *
 * \param tick
 *      The starting tick.
 */

void
midibase::init_clock (midipulse tick)
{
    if (m_clock_type == e_clock_pos && tick != 0)
    {
        continue_from(tick);
    }
    else if (m_clock_type == e_clock_mod || tick == 0)
    {
        start();

        /*
         * The next equation is effectively (m_ppqn / 4) * 16 * 4,
         * or m_ppqn * 16.  Note that later we have pp16th = (m_ppqn / 4).
         */

        midipulse clock_mod_ticks = (m_ppqn / 4) * m_clock_mod;
        midipulse leftover = (tick % clock_mod_ticks);
        midipulse starting_tick = tick - leftover;

        /*
         * Was there anything left? Then wait for next beat (16th note)
         * to start clocking.
         */

        if (leftover > 0)
            starting_tick += clock_mod_ticks;

        m_lasttick = starting_tick - 1;
    }
}

/**
 *  Continue from the given tick.  Tell the device that we are going to start
 *  at a certain position (starting_tick).  If there is anything left, then
 *  wait for next beat (16th note) to start clocking.
 *
 * \param tick
 *      The continuing tick.
 */

void
midibase::continue_from (midipulse tick)
{
    midipulse pp16th = m_ppqn / 4;
    midipulse leftover = tick % pp16th;
    midipulse beats = tick / pp16th;
    midipulse starting_tick = tick - leftover;
    if (leftover > 0)
        starting_tick += pp16th;

    m_lasttick = starting_tick - 1;
    if (m_clock_type != e_clock_off)
    {
        api_continue_from(tick, beats);
    }
}

/**
 *  This function gets the MIDI clock a-runnin', if the clock type is not
 *  e_clock_off.
 */

void
midibase::start ()
{
    m_lasttick = -1;
    if (m_clock_type != e_clock_off)
    {
        api_start();
    }
}

/**
 *  Set status to of "inputting" to the given value.  If the parameter is
 *  true, then init_in() is called; otherwise, deinit_in() is called.
 *
 * \param inputing
 *      The inputing value to set.
 */

void
midibase::set_input (bool inputing)     // not part of portmidi
{
    if (m_inputing != inputing)
    {
        m_inputing = inputing;
        if (inputing)
            init_in();
        else
            deinit_in();
    }
}

/**
 *  Stop the MIDI buss.
 */

void
midibase::stop ()
{
    m_lasttick = -1;
    if (m_clock_type != e_clock_off)
    {
        api_stop();
    }
}

/**
 *  Generates the MIDI clock, starting at the given tick value.
 *
 * \threadsafe
 *
 * \param tick
 *      Provides the starting tick.
 */

void
midibase::clock (midipulse tick)
{
    automutex locker(m_mutex);
    if (m_clock_type != e_clock_off)
    {
        bool done = m_lasttick >= tick;
        int ct = clock_ticks_from_ppqn(m_ppqn);         /* ppqn / 24    */
        while (! done)
        {
            ++m_lasttick;
            done = m_lasttick >= tick;
            if ((m_lasttick % ct) == 0)                 /* tick time?           */
            {
                api_clock(tick);
            }
        }
        api_flush();            /* and send out */
    }
}

}           // namespace seq64

/*
 * midibase.cpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */
