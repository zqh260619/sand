#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// sand.h - 沙堆元胞自动机 DLL 的公共 C API
//
// 规则见项目 README.md：
//   * L x L 正方形网格，开放边界（越界高度固定为 0）
//   * theta = 2
//   * 完全同步松弛：每一轮中所有不稳定格基于同一份旧快照制定倒塌计划
//   * 无 q 的水填充倒塌计划：向所有并列最低方向平均搬运，直到
//     H - min(levels) <= theta
//   * 只有系统稳定（IDLE）时才允许落下下一粒沙
// ---------------------------------------------------------------------------

#ifdef _WIN32
#ifdef SAND_EXPORTS
#define SAND_API __declspec(dllexport)
#else
#define SAND_API __declspec(dllimport)
#endif
#else
#define SAND_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明句柄。 */
typedef struct sand_pile sand_pile;

/*
 * 错误码 / 返回值约定
 *
 * sand_create          : 失败返回 NULL
 * sand_size            : 失败返回 -1
 * sand_get_height      : 失败返回 -1（合法高度均 >= 0）
 * sand_set_height      : 成功返回 0，参数错误返回 -1
 * sand_reset           : 成功返回 0，参数错误返回 -1
 * sand_is_stable       : 稳定返回 1，不稳定返回 0，参数错误返回 -1
 * sand_relax           : 返回本次松弛移动的沙粒总数（雪崩大小，含流出系统），
 *                        参数错误返回 -1
 * sand_relax_grid      : 同 sand_relax，直接操作调用方提供的扁平一维数组
 * sand_update          : 成功返回本次落沙触发的雪崩大小（>= 0）；
 *                        参数错误返回 -1；系统尚不稳定（RELAXING）返回 -2
 * sand_update_grid     : 同 sand_update，直接操作调用方提供的扁平一维数组
 * sand_drop_grain      : sand_update 的别名
 * sand_update_height   : sand_update 的别名
 * update_sand_height   : sand_update_grid 的别名
 * update_height        : sand_update_grid 的别名
 */
#define SAND_ERR_INVALID_ARGUMENT (-1)
#define SAND_ERR_NOT_STABLE       (-2)

/* 创建 L x L 的全零沙堆。L 必须为正整数。 */
SAND_API sand_pile* sand_create(int size);

/* 销毁沙堆。 */
SAND_API void sand_destroy(sand_pile* sand);

/* 将整个网格清零。成功返回 0。 */
SAND_API int sand_reset(sand_pile* sand);

/* 返回网格边长 L；失败返回 -1。 */
SAND_API int sand_size(const sand_pile* sand);

/* 读取 (row, col) 处高度；失败返回 -1。 */
SAND_API int sand_get_height(const sand_pile* sand, int row, int col);

/* 设置 (row, col) 处高度（>= 0）。成功返回 0。 */
SAND_API int sand_set_height(sand_pile* sand, int row, int col, int height);

/* 判断系统是否稳定（不存在 h(c) - h(n) > 2）。 */
SAND_API int sand_is_stable(const sand_pile* sand);

/*
 * 执行完全同步松弛，直到系统稳定。
 * 返回本次松弛中实际移动的沙粒总数（包括流出系统的沙粒）。
 */
SAND_API long long sand_relax(sand_pile* sand);

/*
 * 直接对长度为 size*size 的扁平一维高度数组（行优先）执行完全同步松弛。
 * 返回本次松弛移动的沙粒总数；参数错误返回 -1。
 */
SAND_API long long sand_relax_grid(int* heights, int size);

/*
 * 在 (row, col) 处落一粒沙，并执行完全同步松弛。
 * 若调用时系统尚不稳定，拒绝落沙并返回 SAND_ERR_NOT_STABLE。
 * 成功返回本次落沙触发的雪崩大小。
 */
SAND_API long long sand_update(sand_pile* sand, int row, int col);

/*
 * 直接操作长度为 size*size 的扁平一维高度数组（行优先）。
 * 在 (row, col) 处落一粒沙并完全松弛；要求调用时系统稳定。
 * 成功返回本次落沙触发的雪崩大小。
 */
SAND_API long long sand_update_grid(int* heights, int size, int row, int col);

/* 别名，便于按习惯调用。 */
SAND_API long long sand_drop_grain(sand_pile* sand, int row, int col);
SAND_API long long sand_update_height(sand_pile* sand, int row, int col);
SAND_API long long update_sand_height(int* heights, int size, int row, int col);
SAND_API long long update_height(int* heights, int size, int row, int col);

#ifdef __cplusplus
}
#endif
