#include "pch.h"
#include "sand.h"

// ---------------------------------------------------------------------------
// sand_pile 的定义（头文件中仅前置声明为不透明句柄）。
// ---------------------------------------------------------------------------
struct sand_pile
{
    int size;
    std::vector<int> heights;
};


namespace
{
    // README 第 2 节：临界高差 theta = 2。
    constexpr int kTheta = 2;

    // 四个冯·诺依曼方向。
    enum Direction
    {
            kUp = 0,
            kRight = 1,
            kDown = 2,
            kLeft = 3
    };

    constexpr int kDirectionCount = 4;

    bool in_bounds(int size, int row, int col)
    {
        return row >= 0 && row < size && col >= 0 && col < size;
    }

    std::size_t cell_index(int size, int row, int col)
    {
        return static_cast<std::size_t>(row) * static_cast<std::size_t>(size) +
               static_cast<std::size_t>(col);
    }

    bool valid_pile(const sand_pile* sand)
    {
        return sand != nullptr && sand->size > 0 &&
               sand->heights.size() ==
                   static_cast<std::size_t>(sand->size) *
                       static_cast<std::size_t>(sand->size);
    }

    // README 第 3 节：存在邻居 n，使 h(c) - h(n) > theta 时不稳。
    // 越界邻居高度固定为 0。
    bool unstable_at(const sand_pile* sand, int row, int col)
    {
        const int size = sand->size;
        const auto& h = sand->heights;
        const std::size_t idx = cell_index(size, row, col);
        const int value = h[idx];

        const int up = row > 0 ? h[idx - static_cast<std::size_t>(size)] : 0;
        const int down = row + 1 < size ? h[idx + static_cast<std::size_t>(size)] : 0;
        const int left = col > 0 ? h[idx - 1] : 0;
        const int right = col + 1 < size ? h[idx + 1] : 0;

        return value - up > kTheta || value - down > kTheta ||
               value - left > kTheta || value - right > kTheta;
    }

    bool stable_pile(const sand_pile* sand)
    {
        for (int row = 0; row < sand->size; ++row)
        {
            for (int col = 0; col < sand->size; ++col)
            {
                if (unstable_at(sand, row, col))
                {
                    return false;
                }
            }
        }
        return true;
    }

    // README 第 6 节：无 q 的水填充倒塌计划。
    //
    // 对旧快照上的一个不稳定格，重复向所有并列最低方向各发一粒沙；
    // 内部邻居的“水位”随之 +1，越界方向保持 0 且永远不升高。
    // 剩余沙粒不足一组时，按旋转后的确定性方向顺序逐粒发出。
    std::array<long long, kDirectionCount> collapse_plan(const sand_pile* sand,
                                                         int row,
                                                         int col)
    {
        const int size = sand->size;
        const auto& h = sand->heights;
        const std::size_t idx = cell_index(size, row, col);

        std::array<long long, kDirectionCount> levels{};
        std::array<bool, kDirectionCount> inside{};

        if (row > 0)
        {
            levels[kUp] = h[idx - static_cast<std::size_t>(size)];
            inside[kUp] = true;
        }
        if (col + 1 < size)
        {
            levels[kRight] = h[idx + 1];
            inside[kRight] = true;
        }
        if (row + 1 < size)
        {
            levels[kDown] = h[idx + static_cast<std::size_t>(size)];
            inside[kDown] = true;
        }
        if (col > 0)
        {
            levels[kLeft] = h[idx - 1];
            inside[kLeft] = true;
        }

        long long height = h[idx];
        std::array<long long, kDirectionCount> alloc{};

        while (height - *std::min_element(levels.begin(), levels.end()) > kTheta)
        {
            const long long min_level = *std::min_element(levels.begin(), levels.end());

            int min_directions[kDirectionCount];
            int group_size = 0;
            for (int dir = 0; dir < kDirectionCount; ++dir)
            {
                if (levels[dir] == min_level)
                {
                    min_directions[group_size++] = dir;
                }
            }

            if (height >= group_size)
            {
                // 沙粒足够给每个并列最低方向各一粒。
                for (int i = 0; i < group_size; ++i)
                {
                    const int dir = min_directions[i];
                    ++alloc[dir];
                    if (inside[dir])
                    {
                        ++levels[dir];
                    }
                }
                height -= group_size;
            }
            else
            {
                // 沙粒不足一组：按旋转后的确定性方向顺序逐粒发出。
                // 基础顺序为 上、右、下、左；旋转量由格子坐标确定，
                // 保证遍历 U 的顺序不影响结果且全程无随机。
                constexpr int order[kDirectionCount] = {kUp, kRight, kDown, kLeft};
                const int start = (row + col) & 3;
                long long remaining = height;

                for (int step = 0; step < kDirectionCount && remaining > 0; ++step)
                {
                    const int dir = order[(start + step) & 3];
                    if (levels[dir] == min_level)
                    {
                        ++alloc[dir];
                        --remaining;
                    }
                }
                height = 0;
                break;
            }
        }

        return alloc;
    }

    // README 第 5 节：完全同步松弛。返回本次松弛移动的沙粒总数。
    long long relax_pile(sand_pile* sand)
    {
        const int size = sand->size;
        auto& h = sand->heights;
        const std::size_t cell_count = static_cast<std::size_t>(size) * size;

        long long avalanche_size = 0;
        std::vector<long long> delta(cell_count);
        std::vector<std::size_t> unstable_cells;

        while (true)
        {
            unstable_cells.clear();
            for (int row = 0; row < size; ++row)
            {
                for (int col = 0; col < size; ++col)
                {
                    if (unstable_at(sand, row, col))
                    {
                        unstable_cells.push_back(cell_index(size, row, col));
                    }
                }
            }

            if (unstable_cells.empty())
            {
                break;
            }

            std::fill(delta.begin(), delta.end(), 0);

            // 所有计划都基于本轮开始时的同一份旧快照 h。
            for (const std::size_t idx : unstable_cells)
            {
                const int row = static_cast<int>(idx / static_cast<std::size_t>(size));
                const int col = static_cast<int>(idx % static_cast<std::size_t>(size));
                const auto alloc = collapse_plan(sand, row, col);

                long long sent = 0;
                for (const long long grains : alloc)
                {
                    sent += grains;
                }
                avalanche_size += sent;
                delta[idx] -= sent;

                if (row > 0)
                {
                    delta[idx - static_cast<std::size_t>(size)] += alloc[kUp];
                }
                if (col + 1 < size)
                {
                    delta[idx + 1] += alloc[kRight];
                }
                if (row + 1 < size)
                {
                    delta[idx + static_cast<std::size_t>(size)] += alloc[kDown];
                }
                if (col > 0)
                {
                    delta[idx - 1] += alloc[kLeft];
                }
            }

            // 统一提交本轮所有变化。
            for (std::size_t i = 0; i < cell_count; ++i)
            {
                h[i] += static_cast<int>(delta[i]);
            }
        }

        return avalanche_size;
    }

    long long update_pile(sand_pile* sand, int row, int col)
    {
        if (!valid_pile(sand) || !in_bounds(sand->size, row, col))
        {
            return SAND_ERR_INVALID_ARGUMENT;
        }

        // README 第 4 节硬性约束：上一轮松弛结束前禁止落沙。
        if (!stable_pile(sand))
        {
            return SAND_ERR_NOT_STABLE;
        }

        ++sand->heights[cell_index(sand->size, row, col)];
        return relax_pile(sand);
    }

}  // namespace

extern "C" SAND_API sand_pile* sand_create(int size)
{
    if (size <= 0)
    {
        return nullptr;
    }

    try
    {
        std::vector<int> heights(static_cast<std::size_t>(size) * size, 0);
        return new sand_pile{size, std::move(heights)};
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }
    catch (...)
    {
        return nullptr;
    }
}

extern "C" SAND_API void sand_destroy(sand_pile* sand)
{
    delete sand;
}

extern "C" SAND_API int sand_reset(sand_pile* sand)
{
    if (!valid_pile(sand))
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    std::fill(sand->heights.begin(), sand->heights.end(), 0);
    return 0;
}

extern "C" SAND_API int sand_size(const sand_pile* sand)
{
    if (!valid_pile(sand))
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }
    return sand->size;
}

extern "C" SAND_API int sand_get_height(const sand_pile* sand,
                                        int row,
                                        int col)
{
    if (!valid_pile(sand) || !in_bounds(sand->size, row, col))
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }
    return sand->heights[cell_index(sand->size, row, col)];
}

extern "C" SAND_API int sand_set_height(sand_pile* sand,
                                        int row,
                                        int col,
                                        int height)
{
    if (!valid_pile(sand) || !in_bounds(sand->size, row, col) || height < 0)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    sand->heights[cell_index(sand->size, row, col)] = height;
    return 0;
}

extern "C" SAND_API int sand_is_stable(const sand_pile* sand)
{
    if (!valid_pile(sand))
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }
    return stable_pile(sand) ? 1 : 0;
}

extern "C" SAND_API long long sand_relax(sand_pile* sand)
{
    if (!valid_pile(sand))
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }
    return relax_pile(sand);
}

extern "C" SAND_API long long sand_relax_grid(int* heights, int size)
{
    if (heights == nullptr || size <= 0)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    const std::size_t cell_count = static_cast<std::size_t>(size) * size;
    for (std::size_t i = 0; i < cell_count; ++i)
    {
        if (heights[i] < 0)
        {
            return SAND_ERR_INVALID_ARGUMENT;
        }
    }

    try
    {
        sand_pile pile;
        pile.size = size;
        pile.heights.assign(heights, heights + cell_count);

        const long long avalanche_size = relax_pile(&pile);
        std::copy(pile.heights.begin(), pile.heights.end(), heights);
        return avalanche_size;
    }
    catch (const std::bad_alloc&)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }
    catch (...)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }
}


extern "C" SAND_API long long sand_update(sand_pile* sand, int row, int col)
{
    return update_pile(sand, row, col);
}

extern "C" SAND_API long long sand_update_grid(int* heights,
                                               int size,
                                               int row,
                                               int col)
{
    if (heights == nullptr || size <= 0 || !in_bounds(size, row, col))
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    const std::size_t cell_count = static_cast<std::size_t>(size) * size;
    for (std::size_t i = 0; i < cell_count; ++i)
    {
        if (heights[i] < 0)
        {
            return SAND_ERR_INVALID_ARGUMENT;
        }
    }

    try
    {
        sand_pile pile;
        pile.size = size;
        pile.heights.assign(heights, heights + cell_count);

        const long long result = update_pile(&pile, row, col);
        if (result >= 0)
        {
            std::copy(pile.heights.begin(), pile.heights.end(), heights);
        }
        return result;
    }
    catch (const std::bad_alloc&)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }
    catch (...)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }
}

extern "C" SAND_API long long sand_drop_grain(sand_pile* sand,
                                              int row,
                                              int col)
{
    return sand_update(sand, row, col);
}

extern "C" SAND_API long long sand_update_height(sand_pile* sand,
                                                 int row,
                                                 int col)
{
    return sand_update(sand, row, col);
}

extern "C" SAND_API long long update_sand_height(int* heights,
                                                 int size,
                                                 int row,
                                                 int col)
{
    return sand_update_grid(heights, size, row, col);
}

extern "C" SAND_API long long update_height(int* heights,
                                            int size,
                                            int row,
                                            int col)
{
    return sand_update_grid(heights, size, row, col);
}
