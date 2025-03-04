typedef struct FileTree FileTree;
typedef struct Dir Dir;

Panel  *filetree_create         (Panel *ed_panel, const U8 *working_dir);
void    filetree_update         (Panel *panel);
void    filetree_dir_open       (FileTree *ft, Arena *scratch, Dir *dir);
void    filetree_dir_open_all   (FileTree *ft, Arena *scratch, Dir *dir);
void    filetree_dir_close      (FileTree *ft, Dir *dir);
void    filetree_set_directory  (FileTree *ft, Arena *scratch, const U8 *dirpath);
