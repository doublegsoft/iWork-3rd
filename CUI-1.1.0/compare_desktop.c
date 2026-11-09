#include <cui/cui.h>
#include <cui/cui_app.h>
#include <stdbool.h>
#include <stdio.h>

/* =========================================================================
 * 业务数据结构
 * ========================================================================= */

typedef struct {
  const char* date;
  const char* tx_id;
  const char* account;
  const char* amount;
  const char* status;
  bool is_diff_row;      /* 是否整行标记浅蓝差异背景 */
  bool amount_diff;      /* 金额是否高亮警示红色 */
  bool is_failed;        /* 状态是否为失败 */
} tx_record;

typedef struct {
  const char* type;
  const char* tx_id;
  const char* field;
  const char* sys_a;
  const char* sys_b;
  const char* desc;
  cui_tone tone;
} diff_record;

typedef struct {
  bool font_loaded;
  int sys_a_index;
  int db_a_index;
  int sys_b_index;
  int db_b_index;
  int date_range_index;
} compare_state;

/* 模拟系统 A 流水账数据 */
static const tx_record g_sys_a_rows[] = {
  { "10-01", "TXN1001", "A10001", "1,200.00", "成功", false, false, false },
  { "10-01", "TXN1002", "A10002", "850.00",   "成功", false, false, false },
  { "10-01", "TXN1003", "A10003", "2,300.00", "成功", false, false, false },
  { "10-02", "TXN1004", "A10004", "760.00",   "成功", true,  false, false },
  { "10-02", "TXN1005", "A10005", "120.00",   "失败", false, false, true  },
  { "10-03", "TXN1006", "A10006", "980.00",   "成功", false, false, false },
  { "10-03", "TXN1007", "A10007", "1,500.00", "成功", false, false, false },
  { "10-04", "TXN1008", "A10008", "2,200.00", "成功", false, false, false },
  { "10-04", "TXN1009", "A10009", "670.00",   "成功", false, false, false },
  { "10-05", "TXN1010", "A10010", "300.00",   "成功", false, false, false },
};

/* 模拟系统 B 流水账数据 */
static const tx_record g_sys_b_rows[] = {
  { "10-01", "TXN1001", "A10001", "1,200.00", "成功", false, false, false },
  { "10-01", "TXN1002", "A10002", "850.00",   "成功", false, false, false },
  { "10-01", "TXN1003", "A10003", "2,300.00", "成功", false, false, false },
  { "10-02", "TXN1004", "A10004", "800.00",   "成功", true,  true,  false },
  { "10-02", "TXN1005", "A10005", "120.00",   "成功", false, false, false },
  { "10-03", "TXN1006", "A10006", "980.00",   "成功", false, false, false },
  { "10-03", "TXN1007", "A10007", "1,500.00", "成功", false, false, false },
  { "10-04", "TXN1008", "A10008", "2,200.00", "成功", false, false, false },
  { "10-04", "TXN1009", "A10009", "670.00",   "成功", false, false, false },
  { "10-05", "TXN1010", "A10010", "300.00",   "成功", false, false, false },
};

/* 模拟差异明细列表 */
static const diff_record g_diff_details[] = {
  { "金额不同", "TXN1004", "金额", "760.00", "800.00", "金额不一致", CUI_TONE_DANGER },
  { "状态不同", "TXN1005", "状态", "失败",   "成功",   "状态不一致", CUI_TONE_SUCCESS },
};

/* =========================================================================
 * 中文字体自动加载 (Fallback 机制)
 * ========================================================================= */

static void ensure_cjk_font(cui_ctx* ui, compare_state* state) {
  if (state->font_loaded) {
    return;
  }
  state->font_loaded = true;

  const char* cjk_paths[] = {
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/Hiragino Sans GB.ttc",
    "/System/Library/Fonts/STHeiti Light.ttc",
    "/Library/Fonts/Arial Unicode.ttf"
  };

  for (size_t i = 0; i < sizeof(cjk_paths) / sizeof(cjk_paths[0]); ++i) {
    FILE* f = fopen(cjk_paths[i], "rb");
    if (f) {
      fclose(f);
      cui_font font_id = cui_font_load_file(ui, cjk_paths[i]);
      if (font_id >= 0) {
        cui_font_add_fallback(ui, 0, font_id);
        break;
      }
    }
  }
}

/* =========================================================================
 * 渲染流水数据表格 (系统 A / 系统 B)
 * ========================================================================= */

static void render_tx_table(cui_ctx* ui, const tx_record* rows, int row_count, const char* tbl_id) {
  static const cui_table_column cols[5] = {
    { "日期",   CUI_PX(64),  CUI_TEXT_LEFT,  false },
    { "交易ID", CUI_PX(84),  CUI_TEXT_LEFT,  false },
    { "账户",   CUI_PX(74),  CUI_TEXT_LEFT,  false },
    { "金额",   CUI_FILL,    CUI_TEXT_RIGHT, false },
    { "状态",   CUI_PX(64),  CUI_TEXT_CENTER,false },
  };

  cui_table(ui, tbl_id, cols, 5) {
    for (int i = 0; i < row_count; ++i) {
      const tx_record* r = &rows[i];
      cui_table_row(ui, r->is_diff_row);

      /* 日期 */
      cui_table_cell_text(ui, r->date);

      /* 交易ID */
      cui_table_cell(ui);
      cui_text(ui, r->tx_id, .font = CUI_FONT_MEDIUM);

      /* 账户 */
      cui_table_cell_text(ui, r->account);

      /* 金额 (若不一致则使用浅红高亮文本) */
      cui_table_cell(ui);
      if (r->amount_diff) {
        cui_box(ui, .bg = CUI_RGB(0xfde8e8), .radius = 4, .pad_x = 6, .pad_y = 2) {
          cui_text(ui, r->amount, .tabular = true, .color = CUI_RGB(0xe02424), .font = CUI_FONT_MEDIUM);
        }
      } else {
        cui_text(ui, r->amount, .tabular = true);
      }

      /* 状态 (成功绿 / 失败红) */
      cui_table_cell(ui);
      if (r->is_failed) {
        cui_badge(ui, r->status, .tone = CUI_TONE_DANGER, .size = CUI_SIZE_SM);
      } else {
        cui_badge(ui, r->status, .tone = CUI_TONE_SUCCESS, .size = CUI_SIZE_SM);
      }
    }
  }
}

/* =========================================================================
 * 主渲染循环
 * ========================================================================= */

static void frame(cui_ctx* ui, void* user) {
  compare_state* state = (compare_state*)user;
  ensure_cjk_font(ui, state);

  cui_scroll(ui, .id = "main_scroll", .width = CUI_FILL, .height = CUI_FILL, .fade_edges = true) {
    cui_row(ui, .justify = CUI_JUSTIFY_CENTER, .align = CUI_ALIGN_START, .pad = 20) {
      cui_column(ui, .width = CUI_FILL, .max_width = 1140, .gap = 16) {

        /* =================================================================
         * 1. 顶部 Header 栏 (标题、时间筛选与开始比较按钮)
         * ================================================================= */
        cui_card(ui, .pad_x = 18, .pad_y = 12, .width = CUI_FILL) {
          cui_row(ui, .width = CUI_FILL, .align = CUI_ALIGN_CENTER, .gap = 12) {
            /* 左侧标题与大电脑图标 */
            cui_row(ui, .gap = 10, .align = CUI_ALIGN_CENTER) {
              cui_icon(ui, CUI_ICON_MONITOR, 26, CUI_COLOR_FOREGROUND);
              cui_text(ui, "两套系统流水账比较", .font = CUI_FONT_DISPLAY, .size = 18);
            }

            cui_spacer(ui);

            /* 时间范围筛选 */
            static const char* ranges[] = { "时间范围: 2026-10-01 ~ 2026-10-31" };
            cui_box(ui, .width = CUI_PX(270)) {
              cui_select(ui, "date_range", ranges, 1, &state->date_range_index);
            }

            /* 开始比较主要操作按钮 */
            if (cui_button(ui, "开始比较", .variant = CUI_BUTTON_PRIMARY, .icon = CUI_ICON_PLAY)) {
              cui_toast(ui, "正在重新执行两套系统对账...", .tone = CUI_TONE_INFO);
            }
          }
        }

        /* =================================================================
         * 2. 中间三栏核心区 (系统 A | 比较统计指标 | 系统 B)
         * ================================================================= */
        cui_row(ui, .width = CUI_FILL, .gap = 16, .align = CUI_ALIGN_STRETCH) {

          /* ---------------- 左栏：系统 A ---------------- */
          cui_card(ui, .width = CUI_GROW(1.35f), .pad = 16, .gap = 12) {
            cui_row(ui, .gap = 12, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
              static const char* sys_a_opts[] = { "系统 A" };
              cui_box(ui, .width = CUI_PX(140)) {
                cui_select(ui, "sys_a_select", sys_a_opts, 1, &state->sys_a_index);
              }
              static const char* db_a_opts[] = { "数据库: prod_a" };
              cui_box(ui, .width = CUI_PX(160)) {
                cui_select(ui, "db_a_select", db_a_opts, 1, &state->db_a_index);
              }
            }
            render_tx_table(ui, g_sys_a_rows, 10, "tbl_sys_a");
          }

          /* ---------------- 中栏：比较结果仪表盘 ---------------- */
          cui_card(ui, .width = CUI_PX(180), .pad = 16, .gap = 14, .align = CUI_ALIGN_CENTER) {
            cui_text(ui, "比较结果", .font = CUI_FONT_MEDIUM, .size = 15);

            /* 一致率指标 */
            cui_column(ui, .align = CUI_ALIGN_CENTER, .gap = 2) {
              cui_text(ui, "98%", .font = CUI_FONT_DISPLAY, .size = 36, .color = CUI_RGB(0x1a56db));
              cui_text(ui, "一致率", .size = 13, .color = CUI_COLOR_TEXT_SECONDARY);
            }

            /* 进度条指示 (已完成 98%) */
            cui_progress(ui, 0.98f, .width = CUI_PX(120), .height = 6);

            cui_separator(ui);

            /* 差异计数统计 (修复点：使用 CUI_ALIGN_END) */
            cui_column(ui, .align = CUI_ALIGN_CENTER, .gap = 2) {
              cui_row(ui, .gap = 4, .align = CUI_ALIGN_END) {
                cui_text(ui, "2", .font = CUI_FONT_DISPLAY, .size = 28);
                cui_text(ui, "条", .font = CUI_FONT_MEDIUM, .size = 14);
              }
              cui_text(ui, "差异", .size = 13, .color = CUI_COLOR_TEXT_SECONDARY);
            }

            cui_separator(ui);

            /* 图例 (完全一致、仅 A 有、仅 B 有、金额不同) */
            cui_column(ui, .gap = 8, .align = CUI_ALIGN_START, .pad_left = 6) {
              cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER) {
                cui_icon(ui, CUI_ICON_CIRCLE, 10, CUI_RGB(0x1a56db));
                cui_text(ui, "完全一致", .size = 12);
              }
              cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER) {
                cui_icon(ui, CUI_ICON_CIRCLE, 10, CUI_RGB(0xf59e0b));
                cui_text(ui, "仅 A 有", .size = 12);
              }
              cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER) {
                cui_icon(ui, CUI_ICON_CIRCLE, 10, CUI_RGB(0x10b981));
                cui_text(ui, "仅 B 有", .size = 12);
              }
              cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER) {
                cui_icon(ui, CUI_ICON_CIRCLE, 10, CUI_RGB(0xef4444));
                cui_text(ui, "金额不同", .size = 12);
              }
            }
          }

          /* ---------------- 右栏：系统 B ---------------- */
          cui_card(ui, .width = CUI_GROW(1.35f), .pad = 16, .gap = 12) {
            cui_row(ui, .gap = 12, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
              static const char* sys_b_opts[] = { "系统 B" };
              cui_box(ui, .width = CUI_PX(140)) {
                cui_select(ui, "sys_b_select", sys_b_opts, 1, &state->sys_b_index);
              }
              static const char* db_b_opts[] = { "数据库: prod_b" };
              cui_box(ui, .width = CUI_PX(160)) {
                cui_select(ui, "db_b_select", db_b_opts, 1, &state->db_b_index);
              }
            }
            render_tx_table(ui, g_sys_b_rows, 10, "tbl_sys_b");
          }

        }

        /* =================================================================
         * 3. 底部差异详情卡片
         * ================================================================= */
        cui_card(ui, .pad = 16, .gap = 12, .width = CUI_FILL) {
          cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER) {
            cui_icon(ui, CUI_ICON_CIRCLE_ALERT, 16, CUI_RGB(0x1a56db));
            cui_text(ui, "差异详情", .font = CUI_FONT_MEDIUM, .size = 15);
          }

          static const cui_table_column diff_cols[6] = {
            { "类型",   CUI_PX(90),  CUI_TEXT_LEFT,  false },
            { "交易ID", CUI_PX(120), CUI_TEXT_LEFT,  false },
            { "字段",   CUI_PX(100), CUI_TEXT_LEFT,  false },
            { "系统 A", CUI_PX(140), CUI_TEXT_LEFT,  false },
            { "系统 B", CUI_PX(140), CUI_TEXT_LEFT,  false },
            { "说明",   CUI_FILL,    CUI_TEXT_LEFT,  false },
          };

          cui_table(ui, "tbl_diff_details", diff_cols, 6) {
            for (int i = 0; i < 2; ++i) {
              const diff_record* d = &g_diff_details[i];
              cui_table_row(ui, false);

              /* 类型 Badge */
              cui_table_cell(ui);
              cui_badge(ui, d->type, .tone = d->tone, .size = CUI_SIZE_SM);

              /* 交易ID */
              cui_table_cell(ui);
              cui_text(ui, d->tx_id, .font = CUI_FONT_MEDIUM);

              /* 差异字段 */
              cui_table_cell_text(ui, d->field);

              /* 系统 A 值 */
              cui_table_cell_text(ui, d->sys_a);

              /* 系统 B 值 */
              cui_table_cell_text(ui, d->sys_b);

              /* 说明 */
              cui_table_cell(ui);
              cui_text(ui, d->desc, .color = CUI_COLOR_TEXT_SECONDARY);
            }
          }
        }

      }
    }
  }
}

/* =========================================================================
 * 程序启动入口
 * ========================================================================= */

int main(void) {
  static compare_state state = {
    .font_loaded = false,
    .sys_a_index = 0,
    .db_a_index = 0,
    .sys_b_index = 0,
    .db_b_index = 0,
    .date_range_index = 0
  };

  return cui_app_run(&(cui_app_desc){
    .title = "两套系统流水账比较",
    .width = 1180,
    .height = 780,
    .min_width = 960,
    .min_height = 620
  }, frame, &state);
}
