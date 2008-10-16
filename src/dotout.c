/*
 * Scyther : An automatic verifier for security protocols.
 * Copyright (C) 2007 Cas Cremers
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <limits.h>
#include "system.h"
#include "switches.h"
#include "arachne.h"
#include "binding.h"
#include "depend.h"
#include "type.h"
#include "debug.h"
#include "error.h"
#include "specialterm.h"
#include "compromise.h"
#include "binding.h"
#include "cost.h"

extern Protocol INTRUDER;	// Pointers, to be set by the Init of arachne.c
extern Role I_M;		// Same here.
extern Role I_RRS;
extern Role I_RRSD;
extern Role I_RECEIVE;

#define INVALID		-1
#define isGoal(rd)	(rd->type == READ && !rd->internal)
#define isBound(rd)	(rd->bound)
#define length		step

#define CLAIMTEXTCOLOR "#ffffff"
#define CLAIMCOLOR "#000000"
#define GOODCOMMCOLOR "forestgreen"

#define INTRUDERCOLORH 18.0
#define INTRUDERCOLORL 0.65
#define INTRUDERCOLORS 0.9
#define RUNCOLORL1 0.90
#define RUNCOLORL2 0.65
#define RUNCOLORH1 (INTRUDERCOLORH + 360 - 10.0)
#define RUNCOLORH2 (INTRUDERCOLORH + 10.0)
#define RUNCOLORS1 0.8
#define RUNCOLORS2 0.6
#define RUNCOLORDELTA 0.2	// maximum hue delta between roles (0.2): smaller means role colors of a protocol become more similar.
#define RUNCOLORCONTRACT 0.8	// contract from protocol edges: smaller means more distinction between protocols.
#define UNTRUSTEDCOLORS 0.4
#define COMPROMISEDCOLORS 0.3
#define HELPERCOLORS 0.0

#define CHOOSEWEIGHT "2.0"
#define RUNWEIGHT "10.0"
//#define CHOOSEWEIGHT "1.0"
//#define RUNWEIGHT "1.0"

/*
 * Dot output
 *
 *
 * The algorithm itself is not very complicated; because the semi-bundles have
 * bindings etcetera, a graph can be draw quickly and efficiently.
 *
 * Interesting issues:
 *
 * Binding annotations are only drawn if they don't connect with regular
 * events, and when the item does not occur in any previous binding, it might
 * be connected to the initial intruder knowledge.
 *
 * Color management is quite involved. We draw identical protocols in similar
 * color schemes.  A color scheme is a gradient between two colors, evenly
 * spread over all the runs.
 */

static System sys = NULL;

/*
 * code
 */

//! Is this term chosen by the intruder?
int
isIntruderChoice (const Term t)
{
  if (realTermLeaf (t))
    {
      if (TermRunid (t) >= sys->maxruns)
	{
	  // Chosen by intruder
	  // However, if it is a rolename, this is not really what we mean
	  if (!(t->roleVar || isAgentType (t->stype)))
	    {
	      // Not a role variable, and chosen by the intruder: that's it
	      return true;
	    }
	}
    }
  return false;
}

//! Print the run identifier in some meaningful way
void
printVisualRunID (int rid)
{
  int run;
  int displayi;
  int displayr;
  int display;

  if (rid < sys->maxruns)
    {
      // < sys->maxruns means normal thing (not from makeTraceConcrete)
      displayi = 0;
      displayr = 0;
      for (run = 0; run < rid; run++)
	{
	  if (sys->runs[run].protocol != INTRUDER)
	    {
	      displayr++;
	    }
	  else
	    {
	      displayi++;
	    }
	}
      if (sys->runs[rid].protocol == INTRUDER)
	{
	  display = sys->maxruns + displayi + 1;
	}
      else
	{
	  display = displayr + 1;
	}
      eprintf ("#%i", display);
    }
  else
    {
      eprintf ("%i", (rid - sys->maxruns + 1));
    }
}

void
printVisualRun (const Term t)
{
  if (isIntruderChoice (t))
    {
      eprintf ("Intruder");
    }
  printVisualRunID (TermRunid (t));
}

//! Remap term stuff
void
termPrintRemap (const Term t)
{
  termPrintCustom (t, "", "", "(", ")", "\\{ ", " \\}", printVisualRun);
}

//! Remap term list
void
termlistPrintRemap (Termlist tl, char *sep)
{
  while (tl != NULL)
    {
      termPrintRemap (tl->term);
      tl = tl->next;
      if (tl != NULL)
	{
	  eprintf ("%s", sep);
	}
    }
}

//! Print a term; if it is a variable, show that
void
explainVariable (Term t)
{
  t = deVar (t);
  if (realTermVariable (t))
    {
      eprintf ("any ");
      if (t->roleVar)
	{
	  eprintf ("agent ");
	}
      termPrintRemap (t);
      if (!t->roleVar)
	{
	  if (switches.match == 0 && t->stype != NULL)
	    {
	      Termlist tl;

	      eprintf (" of type ");
	      for (tl = t->stype; tl != NULL; tl = tl->next)
		{
		  termPrintRemap (tl->term);
		  if (tl->next != NULL)
		    {
		      eprintf (",");
		    }
		}
	    }
	}
    }
  else
    {
      termPrintRemap (t);
    }
}


//! Name of intruder node
void
intruderNodeM0 (void)
{
  eprintf ("intruder");
}

//! Draw node
void
node (const System sys, const int run, const int index)
{
  if (sys->runs[run].protocol == INTRUDER)
    {
      if (sys->runs[run].role == I_M)
	{
	  intruderNodeM0 ();
	}
      else
	{
	  eprintf ("ri%i", run);
	}
    }
  else
    {
      if (isHelperProtocol (sys->runs[run].protocol))
	{
	  // Helper protocol is collaped
	  eprintf ("r%i", run);
	}
      else
	{
	  // Regular protocol run
	  if (index == -1)
	    {
	      eprintf ("s%i", run);
	    }
	  else
	    {
	      eprintf ("r%ii%i", run, index);
	    }
	}
    }
}

//! Draw arrow
void
arrow (const System sys, Binding b)
{
  node (sys, b->run_from, b->ev_from);
  eprintf (" -> ");
  node (sys, b->run_to, b->ev_to);
}

//! Redirect node
void
redirNode (const System sys, Binding b)
{
  eprintf ("redir_");
  node (sys, b->run_from, b->ev_from);
  node (sys, b->run_to, b->ev_to);
}

//! Roledef draw
void
roledefDraw (Roledef rd)
{
  void optlabel (void)
  {
    Term label;

    label = rd->label;
    if (label != NULL)
      {
	if (realTermTuple (label))
	  {
	    label = TermOp2 (label);
	  }
	eprintf ("_");
	termPrintRemap (label);
      }
  }

  if (rd->type == READ)
    {
      eprintf ("recv");
      optlabel ();
      eprintf (" from ");
      termPrintRemap (rd->from);
      eprintf ("\\n");
      termPrintRemap (rd->message);
    }
  if (rd->type == SEND)
    {
      if (isTermEqual (rd->label, TERM_Compromise))
	{
	  eprintf ("compromise\\n");
	  termPrintRemap (rd->message);
	}
      else
	{
	  eprintf ("send");
	  optlabel ();
	  eprintf (" to ");
	  termPrintRemap (rd->to);
	  eprintf ("\\n");
	  termPrintRemap (rd->message);
	}
    }
  if (rd->type == CLAIM)
    {
      eprintf ("claim");
      optlabel ();
      eprintf ("\\n");
      termPrintRemap (rd->to);
      if (rd->message != NULL)
	{
	  eprintf (" : ");
	  termPrintRemap (rd->message);
	}
    }
}

//! Choose term node
void
chooseTermNode (const Term t)
{
  eprintf ("CHOOSE");
  {
    char *rsbuf;

    rsbuf = RUNSEP;
    RUNSEP = "x";
    termPrint (t);
    RUNSEP = rsbuf;
  }
}

//! Value for hlsrgb conversion
static double
hlsValue (double n1, double n2, double hue)
{
  if (hue > 360.0)
    hue -= 360.0;
  else if (hue < 0.0)
    hue += 360.0;
  if (hue < 60.0)
    return n1 + (n2 - n1) * hue / 60.0;
  else if (hue < 180.0)
    return n2;
  else if (hue < 240.0)
    return n1 + (n2 - n1) * (240.0 - hue) / 60.0;
  else
    return n1;
}

//! hls to rgb conversion
void
hlsrgbreal (int *r, int *g, int *b, double h, double l, double s)
{
  double m1, m2;

  int bytedouble (double d)
  {
    double x;

    x = 255.0 * d;
    if (x <= 0)
      return 0;
    else if (x >= 255.0)
      return 255;
    else
      return (int) x;
  }

  while (h >= 360.0)
    h -= 360.0;
  while (h < 0)
    h += 360.0;
  m2 = (l <= 0.5) ? (l * (l + s)) : (l + s - l * s);
  m1 = 2.0 * l - m2;
  if (s == 0.0)
    {
      *r = *g = *b = bytedouble (l);
    }
  else
    {
      *r = bytedouble (hlsValue (m1, m2, h + 120.0));
      *g = bytedouble (hlsValue (m1, m2, h));
      *b = bytedouble (hlsValue (m1, m2, h - 120.0));
    }
}

//! hls to rgb conversion
/**
 * Secretly takes the monochrome switch into account
 */
void
hlsrgb (int *r, int *g, int *b, double h, double l, double s)
{
  double closer (double l, double factor)
  {
    return l + ((1.0 - l) * factor);
  }

  if (switches.monochrome)
    {
      // No colors
      s = 0;
      h = 0;
    }

  if (switches.lightness > 0)
    {
      // correction switch for lightness
      if (switches.lightness == 100)
	{
	  l = 1.0;
	}
      else
	{
	  l = closer (l, ((double) switches.lightness / 100.0));
	}
    }

// convert
  hlsrgbreal (r, g, b, h, l, s);
}


//! print color from h,l,s triplet
void
printColor (double h, double l, double s)
{
  int r, g, b;

  hlsrgb (&r, &g, &b, h, l, s);
  eprintf ("#%02x%02x%02x", r, g, b);
}


//! Set local buffer with the correct color for this run.
/**
 * Determines number of protocols, shifts to the right color pair, and colors
 * the run within the current protocol in the fade between the color pair.
 *
 * This can be done much more efficiently by computing these colors once,
 * instead of each time again for each run. However, this is not a
 * speed-critical section so this will do just nicely.
 */
void
setRunColorBuf (const System sys, int run, char *colorbuf)
{
  int range;
  int index;
  double protoffset, protrange;
  double roleoffset, roledelta;
  double color;
  double h, l, s;
  int r, g, b;

  // help function: contract roleoffset, roledelta with a factor (<= 1.0)
  void contract (double factor)
  {
    roledelta = roledelta * factor;
    roleoffset = (roleoffset * factor) + ((1.0 - factor) / 2.0);
  }

  // determine #protocol, resulting in two colors
  {
    Termlist protocols;
    Term refprot;
    int r;
    int firstfound;

    protocols = NULL;
    refprot = sys->runs[run].protocol->nameterm;
    index = 0;
    range = 1;
    firstfound = false;
    for (r = 0; r < sys->maxruns; r++)
      {
	if (sys->runs[r].protocol != INTRUDER)
	  {
	    Term prot;

	    prot = sys->runs[r].protocol->nameterm;
	    if (!isTermEqual (prot, refprot))
	      {
		// Some 'other' protocol
		if (!inTermlist (protocols, prot))
		  {
		    // New other protocol
		    protocols = termlistAdd (protocols, prot);
		    range++;
		    if (!firstfound)
		      {
			index++;
		      }
		  }
	      }
	    else
	      {
		// Our protocol
		firstfound = true;
	      }
	  }
      }
    termlistDelete (protocols);
  }

  // Compute protocol offset [0.0 ... 1.0>
  protrange = 1.0 / range;
  protoffset = index * protrange;

  // We now now our range, and we can determine which role this one is.
  {
    Role rr;
    int done;

    range = 0;
    index = 0;
    done = false;

    for (rr = sys->runs[run].protocol->roles; rr != NULL; rr = rr->next)
      {
	if (sys->runs[run].role == rr)
	  {
	    done = true;
	  }
	else
	  {
	    if (!done)
	      {
		index++;
	      }
	  }
	range++;
      }
  }

  // Compute role offset [0.0 ... 1.0]
  if (range <= 1)
    {
      roledelta = 0.0;
      roleoffset = 0.5;
    }
  else
    {
      // range over 0..1
      roledelta = 1.0 / (range - 1);
      roleoffset = index * roledelta;
      // Now this can result in a delta that is too high (depending on protocolrange)
      if (protrange * roledelta > RUNCOLORDELTA)
	{
	  contract (RUNCOLORDELTA / (protrange * roledelta));
	}
    }

  // We slightly contract the colors (taking them away from protocol edges)
  contract (RUNCOLORCONTRACT);

  // Now we can convert this to a color
  color = protoffset + (protrange * roleoffset);
  h = RUNCOLORH1 + color * (RUNCOLORH2 - RUNCOLORH1);
  l = RUNCOLORL1 + color * (RUNCOLORL2 - RUNCOLORL1);
  s = RUNCOLORS1 + color * (RUNCOLORS2 - RUNCOLORS1);

  // If the run is not trusted, we lower the saturation significantly
  if (!isRunTrusted (sys, run))
    {
      s = UNTRUSTEDCOLORS;
    }

  // If the run is compromised, we lower the saturation significantly
  if (sys->runs[run].protocol->compromiseProtocol)
    {
      s = COMPROMISEDCOLORS;
    }

  // If the protocol is a helper protocol, we revert to graytones
  if (isHelperProtocol (sys->runs[run].protocol))
    {
      s = HELPERCOLORS;
    }

  // set to buffer
  hlsrgb (&r, &g, &b, h, l, s);
  sprintf (colorbuf, "#%02x%02x%02x", r, g, b);

  // compute second color (light version)
  /*
     l += 0.07;
     if (l > 1.0)
     l = 1.0;
   */
  hlsrgb (&r, &g, &b, h, l, s);
  sprintf (colorbuf + 8, "#%02x%02x%02x", r, g, b);
}

//! Communication status
int
isCommunicationExact (const System sys, Binding b)
{
  Roledef rd1, rd2;

  rd1 = eventRoledef (sys, b->run_from, b->ev_from);
  rd2 = eventRoledef (sys, b->run_to, b->ev_to);

  if (!isTermEqual (rd1->message, rd2->message))
    {
      return false;
    }
  if (!isTermEqual (rd1->from, rd2->from))
    {
      return false;
    }
  if (!isTermEqual (rd1->to, rd2->to))
    {
      return false;
    }
  if (!isTermEqual (rd1->label, rd2->label))
    {
      return false;
    }
  return true;
}

//! Ignore some events
int
isEventIgnored (const System sys, int run, int ev)
{
  Roledef rd;

  rd = eventRoledef (sys, run, ev);
  if (rd->type == CLAIM)
    {
      if (run != 0)
	{
	  return true;
	}
      else
	{
	  if (ev != sys->current_claim->ev)
	    {
	      return true;
	    }
	}
    }
  if (isCompromiseEvent (rd))
    {
      // If it has no outgoing arrow, we are not interested in this type of event.
      if (!hasOutgoingBinding (run, ev))
	{
	  return true;
	}
    }
  return false;
}

//! Check whether an event is a function application
int
isApplication (const System sys, const int run)
{
  if (sys->runs[run].protocol == INTRUDER)
    {
      if (sys->runs[run].role == I_RRS)
	{
	  Roledef rd;

	  rd = sys->runs[run].start->next;
	  if (rd != NULL)
	    {
	      if (isTermFunctionName (rd->message))
		{
		  return true;
		}
	    }
	}
    }
  return false;
}

//! Is an event enabled by M0 only?
int
isEnabledM0 (const System sys, const int run, const int ev)
{
  List bl;

  for (bl = sys->bindings; bl != NULL; bl = bl->next)
    {
      Binding b;

      b = (Binding) bl->data;
      if (!b->blocked)
	{
	  // if the binding is not done (class choice) we might
	  // still show it somewhere.
	  if (b->done)
	    {
	      if (b->run_to == run && b->ev_to == ev)
		{
		  if (sys->runs[b->run_from].role != I_M)
		    {
		      return false;
		    }
		}
	    }
	}
    }
  return true;
}

//! Check whether the event is an M_0 function application (special case of the previous)
int
isApplicationM0 (const System sys, const int run)
{
  if (sys->runs[run].length > 1)
    {
      if (isApplication (sys, run))
	{
	  if (isEnabledM0 (sys, run, 0))
	    {
	      if (isEnabledM0 (sys, run, 1))
		{
		  return true;
		}
	    }
	}
    }
  return false;
}

//! Determine ranks for all nodes
/**
 * Some crude algorithm I sketched on the blackboard.
 */
int
graph_ranks (int *ranks, int nodes)
{
  int done;
  int rank;
  int changes;

  int getrank (int run, int ev)
  {
    return ranks[eventNode (run, ev)];
  }

  void setrank (int run, int ev, int rank)
  {
    ranks[eventNode (run, ev)] = rank;
  }

#ifdef DEBUG
  if (hasCycle ())
    {
      error ("Graph ranks tried, but a cycle exists!");
    }
#endif

  {
    int i;

    for (i = 0; i < nodes; i++)
      {
	ranks[i] = INT_MAX;
      }
  }

  rank = 0;
  done = false;
  changes = true;
  while (!done)
    {
      int checkCanEventHappenNow (int run, Roledef rd, int ev)
      {
	//if (sys->runs[run].protocol != INTRUDER)
	{
	  if (getrank (run, ev) == INT_MAX)
	    {
	      // Allright, this regular event is not assigned yet
	      int precevent (int run2, int ev2)
	      {
		//if (sys->runs[run2].protocol != INTRUDER)
		{
		  // regular preceding event
		  int rank2;

		  rank2 = getrank (run2, ev2);
		  if (rank2 > rank)
		    {
		      // higher rank, this cannot be done
		      return false;
		    }
		  if (rank2 == rank)
		    {
		      // equal rank: only if different run
		      if ((sys->runs[run].protocol != INTRUDER)
			  && (run2 == run))
			{
			  return false;
			}
		    }
		}
		return true;
	      }

	      if (iteratePrecedingEvents (sys, precevent, run, ev))
		{
		  // we can do it!
		  changes = true;
		  setrank (run, ev, rank);
		}
	      else
		{
		  done = false;
		}
	    }
	}
	return true;
      }


      if (!changes)
	{
	  rank++;
	  if (rank >= nodes)
	    {
	      warning ("Rank %i increased to the number of nodes %i.", rank,
		       nodes);
	      return rank;
	    }
	}
      done = true;
      changes = false;
      iterateAllEvents (sys, checkCanEventHappenNow);
    }
  return rank;
}

//! Display the ranks
/**
 * Reinstated after it had been gone for a while
 */
void
showRanks (const System sys, const int maxrank, const int *ranks,
	   const int nodes)
{
  int rank;

  //return;

  for (rank = 0; rank <= maxrank; rank++)
    {
      int found;
      int run;

      found = 0;
      for (run = 0; run < sys->maxruns; run++)
	{
	  if (sys->runs[run].protocol != INTRUDER)
	    {
	      int ev;

	      for (ev = 0; ev < sys->runs[run].step; ev++)
		{
		  if (!isEventIgnored (sys, run, ev))
		    {

		      int n;

		      n = eventNode (run, ev);
		      if (ranks[n] == rank)
			{
			  if (found == 0)
			    {
			      eprintf ("\t{ rank = same; ");
			    }
			  node (sys, run, ev);
			  eprintf ("; ");
			  found++;
			}
		    }
		}
	    }
	}
      if (found > 0)
	{
	  eprintf ("}\n");
	}
    }
}

//! Does a term occur in a run?
int
termOccursInRun (Term t, int run)
{
  Roledef rd;
  int e;

  rd = sys->runs[run].start;
  e = 0;
  while (e < sys->runs[run].step)
    {
      if (roledefSubTerm (rd, t))
	{
	  return true;
	}
      e++;
      rd = rd->next;
    }
  return false;
}

//! Draw a class choice
/**
 * \rho classes are already dealt with in the headers, so we should ignore them.
 */
void
drawClass (const System sys, Binding b)
{
  Term varterm;

  // now check in previous things whether we saw that term already
  int notSameTerm (Binding b2)
  {
    return (!isTermEqual (varterm, b2->term));
  }

  varterm = deVar (b->term);

  // Variable?
  if (!isTermVariable (varterm))
    {
      return;
    }

  // Agent variable?
  {
    int run;

    run = TermRunid (varterm);
    if ((run >= 0) && (run < sys->maxruns))
      {
	if (inTermlist (sys->runs[run].rho, varterm))
	  {
	    return;
	  }
      }
  }

  // Seen before?
  if (!iterate_preceding_bindings (b->run_to, b->ev_to, notSameTerm))
    {
      // We saw the same term before. Exit.
      return;
    }




  // not seen before: choose class
  eprintf ("\t");
  chooseTermNode (varterm);
  eprintf (" [label=\"");
  explainVariable (varterm);
  eprintf ("\"];\n");
  eprintf ("\t");
  chooseTermNode (varterm);
  eprintf (" -> ");
  node (sys, b->run_to, b->ev_to);
  eprintf (" [weight=\"%s\",arrowhead=\"none\",style=\"dotted\"];\n",
	   CHOOSEWEIGHT);
}

//! Check for regular send/read events
int
sendReadEvent (Roledef rd)
{
  if (rd->type == READ)
    {
      return true;
    }
  if (rd->type == SEND)
    {
      if (!isTermEqual (rd->label, TERM_Compromise))
	{
	  return true;
	}
    }
  return false;
}

//! Print label of a regular->regular transition node (when comm. is not exact)
/**
 * Note that we ignore any label differences, these are left implicit
 */
void
regularModifiedLabel (Binding b)
{
  Roledef rdfrom;
  Roledef rdto;
  int unknown;

  rdfrom = eventRoledef (sys, b->run_from, b->ev_from);
  rdto = eventRoledef (sys, b->run_to, b->ev_to);
  unknown = true;

  // First up: compare messages contents': what was sent, what is needed
  if (!isTermEqual (rdfrom->message, b->term))
    {
      // What is sent is not equal to what is bound
      if (termInTerm (rdfrom->message, b->term))
	{
	  // Interm: simple select
	  unknown = false;
	  eprintf ("select ");
	  termPrintRemap (b->term);
	  eprintf ("\\n");
	}
    }

  // Second: agent things
  if (sendReadEvent (rdfrom) && sendReadEvent (rdto))
    {
      if (!isTermEqual (rdfrom->from, rdto->from))
	{
	  unknown = false;
	  eprintf ("fake sender ");
	  termPrintRemap (rdto->from);
	  eprintf ("\\n");
	}
      else
	{
	  if (!isTermEqual (rdfrom->to, rdto->to))
	    {
	      unknown = false;
	      eprintf ("redirect to ");
	      termPrintRemap (rdto->to);
	      eprintf ("\\n");
	    }
	  else
	    {
	      if (!isTermEqual (rdfrom->label, rdto->label))
		{
		  unknown = false;
		  eprintf ("redirect");
		  eprintf ("\\n");
		}
	    }
	}
    }

  // Any leftovers for which I don't have a good name yet.
  if (unknown)
    {
      // I'm not quite sure, just an arrow for now.
      eprintf ("use");
      if ((!isTermEqual (rdfrom->message, b->term))
	  && (!isTermEqual (b->term, rdto->message)))
	{
	  eprintf (" ");
	  termPrintRemap (b->term);
	}
      eprintf ("\\n");
    }
}

//! Check whether an event is regular
/**
 * Here, regular means:
 * - not intruder
 * - not helper
 * - not compromised
 */
int
isRegularEvent (const int run, const int ev)
{
  Protocol prot;

  prot = sys->runs[run].protocol;
  if (sys->runs[run].protocol == INTRUDER)
    {
      return false;
    }
  if (isHelperProtocol (prot))
    {
      return false;
    }
  if (isCompromiseEvent (eventRoledef (sys, run, ev)))
    {
      return false;
    }
  return true;
}

//! Draw a single binding
void
drawBinding (const System sys, Binding b)
{
  int intr_to, intr_from, m0_from;

  void myarrow (const Binding b)
  {
    if (m0_from)
      {
	eprintf ("\t");
	intruderNodeM0 ();
	eprintf (" -> ");
	node (sys, b->run_to, b->ev_to);
      }
    else
      {
	arrow (sys, b);
      }

  }

  intr_from = (!isRegularEvent (b->run_from, b->ev_from));
  intr_to = (!isRegularEvent (b->run_to, b->ev_to));
  m0_from = false;

  // Pruning: things going to M0 applications are pruned;
  if (isApplicationM0 (sys, b->run_to))
    {
      return;
    }
  if (isApplicationM0 (sys, b->run_from) ||
      sys->runs[b->run_from].role == I_M)
    {
      m0_from = true;
    }

  // Normal drawing cases;
  if (intr_from)
    {
      // from intruder
      /*
       * Because this can be generated many times, it seems
       * reasonable to not duplicate such arrows, especially when
       * they're from M_0. Maybe the others are still relevant.
       */
      if (1 == 1 || sys->runs[b->run_from].role == I_M)
	{
	  // now check in previous things whether we saw that term already
	  int notSameTerm (Binding b2)
	  {
	    return (!isTermEqual (b->term, b2->term));
	  }

	  if (!iterate_preceding_bindings (b->run_to, b->ev_to, notSameTerm))
	    {
	      // We saw the same term before. Exit.
	      return;
	    }
	}

      // normal from intruder, not seen before (might be M_0)
      if (intr_to)
	{
	  // intr->intr
	  Term agent;

	  eprintf ("\t");
	  myarrow (b);
	  agent = getPrivateKeyAgent (b);
	  if (agent != NULL)
	    {
	      // It's a private key transfer, mark clearly
	      eprintf (" [label=\"[ ");
	      termPrintRemap (b->term);
	      eprintf (" ]\"");
	      eprintf (",fontcolor=\"red\"");
	    }
	  else
	    {
	      eprintf (" [label=\"");
	      termPrintRemap (b->term);
	      eprintf ("\"");
	    }

	  if (m0_from)
	    {
	      eprintf (",weight=\"10.0\"");
	    }
	  eprintf ("]");
	  eprintf (";\n");
	}
      else
	{
	  // intr->regular
	  eprintf ("\t");
	  myarrow (b);
	  if (m0_from)
	    {
	      eprintf ("[weight=\"0.5\"]");
	    }
	  eprintf (";\n");
	}
    }
  else
    {
      // not from intruder
      if (intr_to)
	{
	  // regular->intr
	  eprintf ("\t");
	  myarrow (b);
	  eprintf (";\n");
	}
      else
	{
	  // regular->regular
	  /*
	   * Has this been done *exactly* as we hoped?
	   */
	  if (isCommunicationExact (sys, b))
	    {
	      eprintf ("\t");
	      myarrow (b);
	      eprintf (" [style=bold,color=\"%s\"]", GOODCOMMCOLOR);
	      eprintf (";\n");
	    }
	  else
	    {
	      // Something was changed, so we call this a redirect
	      eprintf ("\t");
	      node (sys, b->run_from, b->ev_from);
	      eprintf (" -> ");
	      redirNode (sys, b);
	      eprintf (" -> ");
	      node (sys, b->run_to, b->ev_to);
	      eprintf (";\n");

	      eprintf ("\t");
	      redirNode (sys, b);
	      eprintf (" [style=filled,fillcolor=\"");
	      printColor (INTRUDERCOLORH, INTRUDERCOLORL, INTRUDERCOLORS);
	      eprintf ("\",label=\"");
	      regularModifiedLabel (b);
	      eprintf ("\"]");
	      eprintf (";\n");

	    }
	}
    }
}

//! Draw dependecies (including intruder!)
/**
 * Returns from_intruder_count (from M_0)
 */
int
drawAllBindings (const System sys)
{
  List bl;
  List bldone;
  int fromintr;

  bldone = NULL;
  fromintr = 0;
  for (bl = sys->bindings; bl != NULL; bl = bl->next)
    {
      Binding b;

      b = (Binding) bl->data;
      if (!b->blocked)
	{
	  // if the binding is not done (class choice) we might
	  // still show it somewhere.
	  if (b->done)
	    {
	      // Check whether we already drew it
	      List bl2;
	      int drawn;

	      drawn = false;
	      for (bl2 = bldone; bl2 != NULL; bl2 = bl2->next)
		{
		  if (same_binding (b, (Binding) bl2->data))
		    {
		      drawn = true;
		      break;
		    }
		}

	      if (!drawn)
		{
		  // done, draw
		  drawBinding (sys, b);

		  // from intruder?
		  if (sys->runs[b->run_from].protocol == INTRUDER)
		    {
		      if (sys->runs[b->run_from].role == I_M)
			{
			  fromintr++;
			}
		    }
		  // Add to drawn list
		  bldone = list_add (bldone, b);
		}
	    }
	  else
	    {
	      drawClass (sys, b);
	    }
	}
    }
  list_destroy (bldone);	// bindings list
  return fromintr;
}

//! Print "Alice in role R" of a run
void
printAgentInRole (const System sys, const int run)
{
  Term rolename;
  Term agentname;

  rolename = sys->runs[run].role->nameterm;
  agentname = agentOfRunRole (sys, run, rolename);
  explainVariable (agentname);
  eprintf (" in role ");
  termPrintRemap (rolename);
}

//! rho, sigma, const
/* 
 * true if it has printed
   */
int
showLocal (const int run, Term told, Term tnew, char *prefix, char *cursep)
{
  if (realTermVariable (tnew))
    {
      if (termOccursInRun (tnew, run))
	{
	  // Variables are mapped, maybe. But then we wonder whether they occur in reads.
	  eprintf (cursep);
	  eprintf (prefix);
	  termPrintRemap (told);
	  eprintf (" -\\> ");
	  explainVariable (tnew);
	}
      else
	{
	  return false;
	}
    }
  else
    {
      eprintf (cursep);
      eprintf (prefix);
      termPrintRemap (tnew);
    }
  return true;
}


//! show a list of locals
/**
 * never ends with the seperator
 */
int
showLocals (const int run, Termlist tlold, Termlist tlnew,
	    Term tavoid, char *prefix, char *sep)
{
  int anything;
  char *cursep;

  cursep = "";
  anything = false;
  while (tlold != NULL && tlnew != NULL)
    {
      if (!isTermEqual (tlold->term, tavoid))
	{
	  if (showLocal (run, tlold->term, tlnew->term, prefix, cursep))
	    {
	      cursep = sep;
	      anything = true;
	    }
	}
      tlold = tlold->next;
      tlnew = tlnew->next;
    }
  return anything;
}

//! Explain the local constants
/**
 * Return true iff something was printed
 */
int
printRunConstants (const System sys, const int run)
{
  if (sys->runs[run].constants != NULL)
    {
      eprintf ("Const ");
      showLocals (run, sys->runs[run].role->
		  declaredconsts, sys->runs[run].constants, NULL, "", ", ");
      eprintf ("\\l");
      return true;
    }
  else
    {
      return false;
    }
}


//! Explain a run in two lines
void
printRunExplanation (const System sys, const int run,
		     char *runrolesep, char *newline)
{
  int hadcontent;

  eprintf ("Run ");
  printVisualRunID (run);

  eprintf (runrolesep);
  // Print first line
  printAgentInRole (sys, run);
  eprintf ("\\l");

  // Second line

  // Possible protocol (if more than one)
  {
    int showprotocol;
    Protocol p;
    int morethanone;

    // Simple case: don't show
    showprotocol = false;

    // Check whether the protocol spec has more than one
    morethanone = false;
    for (p = sys->protocols; p != NULL; p = p->next)
      {
	if (p != INTRUDER)
	  {
	    if (p != sys->runs[run].protocol)
	      {
		morethanone = true;
		break;
	      }
	  }
      }

    // More than one?
    if (morethanone)
      {
	// This used to work for run 0 always...
	//if (run == 0)
	if (false)
	  {
	    // If this is run 0 we report the protocol anyway, even is there is only a single one in the attack
	    showprotocol = true;
	  }
	else
	  {
	    int r;
	    // For other runs we only report when there are multiple protocols
	    showprotocol = false;
	    for (r = 0; r < sys->maxruns; r++)
	      {
		if (sys->runs[r].protocol != INTRUDER)
		  {
		    if (sys->runs[r].protocol != sys->runs[run].protocol)
		      {
			showprotocol = true;
			break;
		      }
		  }
	      }
	  }
      }

    // Use the result
    if (showprotocol)
      {
	eprintf ("Protocol ");
	termPrintRemap (sys->runs[run].protocol->nameterm);
	eprintf ("\\l");
      }
  }

  // Partner or compromise? (if needed to show this)
  if (switches.LKRaftercorrect || switches.LKRrnsafe || switches.SSR || switches.SKR)
    {
      if (sys->runs[run].partner)
	{
	  eprintf ("(partner)");
	  eprintf ("\\l");
	}
    }
  if (sys->runs[run].protocol->compromiseProtocol)
    {
      eprintf ("(compromise %i)", sys->runs[run].protocol->compromiseProtocol);
      eprintf ("\\l");
    }

  eprintf (newline);
  hadcontent = false;

  {
    /*
     * Originally, we ignored the actor in the rho list, but for more than two-party protocols, this was unclear.
     */
    int numroles;
    int ignoreactor;

    ignoreactor = false;	// set to true to ignore the actor
    numroles = termlistLength (sys->runs[run].rho);

    if (numroles > 1)
      {
	{
	  Term ignoreterm;

	  if (ignoreactor)
	    {
	      ignoreterm = sys->runs[run].role->nameterm;
	    }
	  else
	    {
	      ignoreterm = NULL;
	    }
	  hadcontent = showLocals (run, sys->runs[run].protocol->
				   rolenames, sys->runs[run].rho,
				   ignoreterm, "", "\\l");
	}
      }
  }

  if (hadcontent)
    {
      eprintf ("\\l");
      eprintf (newline);
      hadcontent = false;
    }
  hadcontent = printRunConstants (sys, run);

  if (sys->runs[run].sigma != NULL)
    {
      if (hadcontent)
	{
	  eprintf (newline);
	  hadcontent = false;
	}
      if (showLocals (run, sys->runs[run].role->
		      declaredvars, sys->runs[run].sigma, NULL, "Var ",
		      "\\l"))
	{
	  eprintf ("\\l");
	}
    }
}

//! Draw regular runs
void
drawRegularRuns (const System sys)
{
  int run;
  int rcnum;
  char *colorbuf;

  // two buffers, eight chars each
  colorbuf = malloc (16 * sizeof (char));

  rcnum = 0;
  for (run = 0; run < sys->maxruns; run++)
    {
      if (sys->runs[run].length > 0)
	{
	  if (sys->runs[run].protocol != INTRUDER)
	    {
	      if (isHelperProtocol (sys->runs[run].protocol))
		{
		  /**
		   * Helper protocol run
		   *
		   * Collapse into single node
		   */
		  // Draw the first box (HEADER)
		  // This used to be drawn only if done && send_before_read, now we always draw it.
		  setRunColorBuf (sys, run, colorbuf);
		  eprintf ("\t\tr%i [label=\"", run);
		  termPrintRemap (sys->runs[run].protocol->nameterm);
		  eprintf (", ");
		  termPrintRemap (sys->runs[run].role->nameterm);
		  // close up
		  eprintf ("\",shape=box,style=filled,fillcolor=\"%s\"",
			   colorbuf + 8);
		  eprintf ("];\n");
		}
	      else
		{
		  /**
		   * Regular protocol run
		   */
		  Roledef rd;
		  int index;
		  int prevnode;
		  int firstnode;

		  prevnode = -1;
		  index = 0;
		  firstnode = true;
		  rd = sys->runs[run].start;
		  // Regular run

		  if (switches.clusters)
		    {
		      eprintf ("\tsubgraph cluster_run%i {\n", run);

		      eprintf ("\t\tstyle=filled;\n");
		      eprintf ("\t\tcolor=lightgrey;\n");

		      eprintf ("\t\tlabel=\"");
		      printRunExplanation (sys, run, " : ", "");
		      eprintf ("\";\n\n");
		    }

		  // set color
		  setRunColorBuf (sys, run, colorbuf);

		  // Display the respective events
		  while (index < sys->runs[run].length)
		    {
		      if (!isEventIgnored (sys, run, index))
			{
			  // Print node itself
			  eprintf ("\t\t");
			  node (sys, run, index);
			  eprintf (" [");
			  if (run == 0 && index == sys->current_claim->ev)
			    {
			      // The claim under scrutiny
			      eprintf
				("style=filled,fontcolor=\"%s\",fillcolor=\"%s\",shape=box,",
				 CLAIMTEXTCOLOR, CLAIMCOLOR);
			    }
			  else
			    {
			      eprintf ("style=filled,");
			      // print color and shape of this run
			      eprintf ("fillcolor=\"");
			      if (isCompromiseEvent (rd))
				{
				  printColor (INTRUDERCOLORH, INTRUDERCOLORL,
					      INTRUDERCOLORS);
				  eprintf ("\", ");
				}
			      else
				{
				  eprintf ("%s", colorbuf);
				  eprintf ("\", shape=box, ");
				}
			    }
			  eprintf ("label=\"");
			  if (isHelperProtocol (sys->runs[run].protocol) &&
			      ((rd->type == READ) || (rd->type == SEND)))
			    {
			      termPrintRemap (sys->runs[run].protocol->
					      nameterm);
			      eprintf (", ");
			      termPrintRemap (sys->runs[run].role->nameterm);
			      eprintf (": ");
			      if (rd->type == SEND)
				{
				  eprintf ("send ");
				}
			      else
				{
				  eprintf ("recv ");
				}
			      termPrintRemap (rd->message);

			    }
			  else
			    {
			      //roledefPrintShort (rd);
			      roledefDraw (rd);
			    }
			  eprintf ("\"]");
			  eprintf (";\n");

			  // Print binding to previous node
			  if (!firstnode)
			    {
			      // index > 0
			      eprintf ("\t\t");
			      node (sys, run, prevnode);
			      eprintf (" -> ");
			      node (sys, run, index);
			      eprintf (" [style=\"bold\", weight=\"%s\"]",
				       RUNWEIGHT);
			      eprintf (";\n");
			      prevnode = index;
			    }
			  else
			    {
			      // index == firstReal
			      Roledef rd;
			      int send_before_read;
			      int done;

			      // Determine if it is an active role or note
			  /**
			   *@todo note that this will probably become a standard function call for role.h
			   */
			      rd =
				roledef_shift (sys->runs[run].start,
					       sys->runs[run].firstReal);
			      done = 0;
			      send_before_read = 0;
			      while (!done && rd != NULL)
				{
				  if (rd->type == READ)
				    {
				      done = 1;
				    }
				  if (rd->type == SEND)
				    {
				      done = 1;
				      send_before_read = 1;
				    }
				  rd = rd->next;
				}

			      if (!switches.clusters)
				{
				  // Draw the first box (HEADER)
				  // This used to be drawn only if done && send_before_read, now we always draw it.
				  eprintf ("\t\ts%i [label=\"{ ", run);
				  printRunExplanation (sys, run, "\\l", "|");
				  if (sys->runs[run].role->initiator)
				    {
				      eprintf ("Initiator");
				    }
				  else
				    {
				      eprintf ("Responder");
				    }
				  // close up
				  eprintf ("}\", shape=record");
				  eprintf
				    (",style=filled,fillcolor=\"%s\"",
				     colorbuf + 8);
				  eprintf ("];\n");
				  eprintf ("\t\ts%i -> ", run);
				  node (sys, run, index);
				  eprintf
				    (" [style=bold, weight=\"%s\"];\n",
				     RUNWEIGHT);
				  prevnode = index;
				}
			    }
			  firstnode = false;
			}
		      index++;
		      rd = rd->next;
		    }

		  if (switches.clusters)
		    {
		      eprintf ("\t}\n");
		    }
		}
	    }
	}
    }
  free (colorbuf);
}

//! Draw intruder runs
void
drawIntruderRuns (const System sys)
{
  int run;

  if (switches.clusters)
    {
      //eprintf ("\tsubgraph cluster_intruder {\n");
      eprintf ("\tsubgraph intr {\n");
      eprintf ("\t\tlabel = \"Intruder\";\n");
      eprintf ("\t\tcolor = red;\n");
    }

  for (run = 0; run < sys->maxruns; run++)
    {
      if (sys->runs[run].length > 0)
	{
	  if (sys->runs[run].protocol == INTRUDER)
	    {
	      // Intruder run
	      if (sys->runs[run].role != I_M && !isApplicationM0 (sys, run))
		{
		  // Not an M_0 run, and not an M0 function application, so we can draw it.
		  eprintf ("\t\t");
		  node (sys, run, 0);
		  eprintf (" [style=filled,fillcolor=\"");
		  printColor (INTRUDERCOLORH, INTRUDERCOLORL, INTRUDERCOLORS);
		  eprintf ("\",");
		  if (sys->runs[run].role == I_RRSD)
		    {
		      eprintf ("label=\"decrypt\"");
		    }
		  if (sys->runs[run].role == I_RRS)
		    {
		      // Distinguish function application
		      if (isTermFunctionName
			  (sys->runs[run].start->next->message))
			{
			  eprintf ("label=\"apply\"");
			}
		      else
			{
			  eprintf ("label=\"encrypt\"");
			}
		    }
		  if (sys->runs[run].role == I_RECEIVE)
		    {
		      eprintf ("label=\"learn\"");
		    }
		  eprintf ("];\n");
		}
	    }
	}
    }
  if (switches.clusters)
    {
      eprintf ("\t}\n\n");
    }
}

//! Display the current semistate using dot output format.
/**
 * This is not as nice as we would like it. Furthermore, the function is too big.
 */
void
dotSemiState (const System mysys)
{
  static int attack_number = 0;
  Protocol p;
  int *ranks;
  int maxrank;
  int from_intruder_count;
  int nodes;

  sys = mysys;

  // Open graph
  attack_number++;
  eprintf ("digraph semiState%i {\n", attack_number);
  eprintf ("\tlabel = \"[Id %i] Protocol ", sys->attackid);
  p = (Protocol) sys->current_claim->protocol;
  termPrintRemap (p->nameterm);
  eprintf (", role ");
  termPrintRemap (sys->current_claim->rolename);
  eprintf (", claim type ");
  termPrintRemap (sys->current_claim->type);
  // For debugging:
  eprintf (", cost %i", computeAttackCost (sys));
  eprintf ("\";\n");

  // Needed for the bindings later on: create graph

  nodes = nodeCount ();
  ranks = malloc (nodes * sizeof (int));
  maxrank = graph_ranks (ranks, nodes);	// determine ranks

#ifdef DEBUG
  if (DEBUGL (1))
    {
      // For debugging purposes, we also display an ASCII version of some stuff in the comments
      printSemiState ();
      // Even draw all dependencies for non-intruder runs
      // Real nice debugging :(
      int run;

      run = 0;
      while (run < sys->maxruns)
	{
	  int ev;

	  ev = 0;
	  while (ev < sys->runs[run].length)
	    {
	      int run2;
	      int notfirstrun;

	      eprintf ("// precedence: r%ii%i <- ", run, ev);
	      run2 = 0;
	      notfirstrun = 0;
	      while (run2 < sys->maxruns)
		{
		  int notfirstev;
		  int ev2;

		  notfirstev = 0;
		  ev2 = 0;
		  while (ev2 < sys->runs[run2].length)
		    {
		      if (isDependEvent (run2, ev2, run, ev))
			{
			  if (notfirstev)
			    eprintf (",");
			  else
			    {
			      if (notfirstrun)
				eprintf (" ");
			      eprintf ("r%i:", run2);
			    }
			  eprintf ("%i", ev2);
			  notfirstrun = 1;
			  notfirstev = 1;
			}
		      ev2++;
		    }
		  run2++;
		}
	      eprintf ("\n");
	      ev++;
	    }
	  run++;
	}
    }
#endif

  // First, runs
  drawRegularRuns (sys);
  drawIntruderRuns (sys);
  from_intruder_count = drawAllBindings (sys);

  // Third, the intruder node (if needed)
  {
    /*
     * Stupid brute analysis, can probably be done much more efficient, but
     * this is not a timing critical bit, so we just do it like this.
     */
    Termlist found;
    List bl;

    // collect the intruder-generated constants
    found = NULL;
    for (bl = sys->bindings; bl != NULL; bl = bl->next)
      {
	Binding b;

	b = (Binding) bl->data;
	if (!b->blocked)
	  {
	    int addsubterms (Term t)
	    {
	      if (isIntruderChoice (t))
		{
		  found = termlistAddNew (found, t);
		}
	      return true;
	    }

	    term_iterate_open_leaves (b->term, addsubterms);
	  }
      }

    // now maybe we draw the node
    if ((from_intruder_count > 0) || (found != NULL))
      {
	eprintf ("\tintruder [\n");
	eprintf ("\t\tlabel=\"");
	eprintf ("Initial intruder knowledge");
	if (found != NULL)
	  {
	    eprintf ("\\n");
	    eprintf ("The intruder generates: ");
	    termlistPrintRemap (found, ", ");
	  }
	eprintf ("\",\n");
	eprintf ("\t\tstyle=filled,fillcolor=\"");
	printColor (INTRUDERCOLORH, INTRUDERCOLORL, INTRUDERCOLORS);
	eprintf ("\"\n\t];\n");
      }
    termlistDelete (found);
  }

  // eprintf ("\t};\n");

  // For debugging we might add more stuff: full dependencies
#ifdef DEBUG
  if (DEBUGL (3))
    {
      int r1;

      for (r1 = 0; r1 < sys->maxruns; r1++)
	{
	  if (sys->runs[r1].protocol != INTRUDER)
	    {
	      int e1;

	      for (e1 = 0; e1 < sys->runs[r1].step; e1++)
		{
		  int r2;

		  for (r2 = 0; r2 < sys->maxruns; r2++)
		    {
		      if (sys->runs[r2].protocol != INTRUDER)
			{
			  int e2;

			  for (e2 = 0; e2 < sys->runs[r2].step; e2++)
			    {
			      if (isDependEvent (r1, e1, r2, e2))
				{
				  eprintf
				    ("\tr%ii%i -> r%ii%i [color=grey];\n",
				     r1, e1, r2, e2);
				}
			    }
			}
		    }
		}
	    }
	}
    }
#endif

  // Ranks
  if (switches.clusters)
    {
      showRanks (sys, maxrank, ranks, nodes);
    }

#ifdef DEBUG
  // Debug: print dependencies
  if (DEBUGL (3))
    {
      dependPrint ();
    }
#endif

  // clean memory
  free (ranks);			// ranks

  // close graph
  eprintf ("};\n\n");
}
