void pad_init(void);
void pad_free(void);
void pad_fg(int fg);
void pad_bg(int bg);
void pad_add(int c);
void pad_put(int c, int x, int y);
void pad_blank();
void pad_move(int r, int c);
int pad_row(void);
int pad_col(void);
int pad_rows(void);
int pad_cols(void);
