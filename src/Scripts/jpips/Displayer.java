

/** $Id$
  * $Log: Displayer.java,v $
  * Revision 1.2  1998/06/30 14:57:56  didry
  * do not manage the panel anymore
  *
  * Revision 1.1  1998/06/09 07:28:28  didry
  * Initial revision
  *
  */


package JPips;


import java.awt.swing.*;
import java.io.*;
import java.awt.*;
import JPips.Pawt.*;
import java.awt.swing.border.*;
import java.util.*;


/** A window manager.
  * It manages the displaying of windows on the screen.
  * @author Francois Didry
  */
abstract class Displayer implements JPipsComponent
{


  public  Vector	frameVector;	//contains the displayed windows
  public  int		noWindows;	//number of displayed windows
  public  PPanel	panel;		//panel of the displayer


  /** Sets the number of displayed windows to 0.
    * Creates the displayer panel for JPips.
    */
  public Displayer()
    {
      noWindows = 0;
      frameVector = new Vector();
      buildPanel();
    }
  
  
  /** Builds the panel for JPips.
    */
  public void buildPanel()
    {
      panel = new PPanel(new BorderLayout());
      panel.setPreferredSize(new Dimension(300,150));      
      panel.setBorder(new TitledBorder("Windows"));
    }


  /** Adds a window to the count.
    */
  public void incNoWindows()
    {
      noWindows++;
    }


  /** Removes a window from the count.
    */
  public void decNoWindows()
    {
      noWindows--;
    }


  /** @return the current number of windows
    */
  public int getNoWindows()
    {
      return noWindows;
    }


  /** @return the displayer panel for JPips
    */
  public Component getComponent()
    {
      return panel;
    }


  abstract boolean display(File file, boolean locked, boolean writable);

  abstract void display(String name, String string,
                        boolean locked, boolean writable);

  public PMenu getMenu() { return null; }
  
  public void setActivated(boolean yes) {}

  public void reset() {}


}
