# Edit
A modal text editor!

[![no effort edit demo](https://img.youtube.com/vi/4qpOUSXvTjM/0.jpg)](https://www.youtube.com/watch?v=4qpOUSXvTjM)

Unix only for now.

## Keybinds
Keybinds can only be changed by editing the source.

```
UI STUFF ############################################################
C-w - focus next panel
C-W - focus previous panel
C-q - exit editor. discards all unsaved changes!
C-p - create new editor vsplit
C-m - create new mass search/replace vsplit

EDITOR ##############################################################

MOVEMENT AND SELECTION  ---------------------------------------------
  j - select next group
  k - select previous group

  h - expand selection group
  l - contract selection group
  
  w - contract selection group and select first new group
  W - expand selection group and select first new group
  e - contract selection group and select last new group
  E - expand selection group and select last new group

  K - contract selection upwards
C-J - contract selection downwards
C-K - expand selection upwards

  r - toggle subword selection group and select group at start of selection

  f - select entire file
  F - select from selection start to end of file
C-F - select from start of file to selection end
  a - enter edit mode at end of selection

QUICK MOVE MODE -----------------------------------------------------
C-j - quick move downwards
C-k - quick move upwards
Esc - exit quick move mode and return to original position
Enter - exit quick move mode at current position

SEARCH MODE ---------------------------------------------------------
  / - enter search mode
  
C-r - replace mode

C-j - go to next matched item
C-k - go to previous matched item
Esc - exit search mode
Enter - exit search mode and select current matched item

REPLACE MODE --------------------------------------------------------
Esc - exit replace mode to search mode
Enter - replace all

EDIT MODE -----------------------------------------------------------
  c - delete selection and enter edit mode
  C - trim, delete selection, and enter edit mode

  i - enter edit mode at start of selection
  a - enter edit mode at end of selection

  I - enter edit mode at first non-whitespace character of selection
  A - enter edit mode at last non-whitespace character of selection

COPY PASTE ----------------------------------------------------------
  d - cut selection and select next group
  y - copy selection
  p - paste after selection
  P - paste before selection

MISC ----------------------------------------------------------------
C-s - save
  u - undo
C-r - redo

  < - unindent lines
  > - indent lines
  
  v - comment lines
  V - uncomment lines

  q - close editor if saved
  Q - close editor without saving

  t - open file tree and recursively expand all folders
  T - open file tree
  
FILE TREE ###########################################################
C-R - expand all folders
Esc - exit filetree

    Using the '*' character will enable fuzzy search. 
    You can add it multiple times for fuzzier and fuzzier searching.  

MASS SEARCH / REPLACE ###############################################
```
