#include "pch.h"
#include "sand.h"

namespace
{
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
}  // namespace

// ---------------------------------------------------------------------------
// 沙堆类：封装网格状态、模型参数与全部演化规则。
// 头文件中仅前置声明为不透明句柄。
// ---------------------------------------------------------------------------
class sand_pile
{
public:
    sand_pile(int size, int theta);

    int size() const;
    int theta() const;
    bool set_theta(int theta);
    void reset();

    int get_height(int row, int col) const;
    bool set_height(int row, int col, int height);
    bool is_stable() const;

    long long relax();
    long long update(int row, int col);

    bool load_heights(const int* values, std::size_t count);
    void store_heights(int* values) const;

private:
    bool in_bounds(int row, int col) const;
    std::size_t cell_index(int row, int col) const;
    bool unstable_at(int row, int col) const;
    std::array<long long, kDirectionCount> collapse_plan(int row, int col) const;

    int size_;
    int theta_;
    std::vector<int> heights_;
};

sand_pile::sand_pile(int size, int theta): 
    size_(size), theta_(theta), heights_(static_cast<std::size_t>(size) * size, 0)
{
}

int sand_pile::size() const
{
    return size_;
}

int sand_pile::theta() const
{
    return theta_;
}

bool sand_pile::set_theta(int theta)
{
    if (theta < 0)
    {
        return false;
    }

    theta_ = theta;
    return true;
}

void sand_pile::reset()
{
    std::fill(heights_.begin(), heights_.end(), 0);
}

bool sand_pile::in_bounds(int row, int col) const
{
    return row >= 0 && row < size_ && col >= 0 && col < size_;
}

std::size_t sand_pile::cell_index(int row, int col) const
{
    return static_cast<std::size_t>(row) * static_cast<std::size_t>(size_) +
           static_cast<std::size_t>(col);
}

int sand_pile::get_height(int row, int col) const
{
    if (!in_bounds(row, col))
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    return heights_[cell_index(row, col)];
}

bool sand_pile::set_height(int row, int col, int height)
{
    if (!in_bounds(row, col) || height < 0)
    {
        return false;
    }

    heights_[cell_index(row, col)] = height;
    return true;
}

bool sand_pile::load_heights(const int* values, std::size_t count)
{
    if (values == nullptr || count != heights_.size())
    {
        return false;
    }

    heights_.assign(values, values + count);
    return true;
}

void sand_pile::store_heights(int* values) const
{
    std::copy(heights_.begin(), heights_.end(), values);
}

// README 第 3 节：存在邻居 n，使 h(c) - h(n) > theta 时不稳。
// 越界邻居高度固定为 0。
bool sand_pile::unstable_at(int row, int col) const
{
    const auto& h = heights_;
    const std::size_t idx = cell_index(row, col);
    const int value = h[idx];

    const int up = row > 0 ? h[idx - static_cast<std::size_t>(size_)] : 0;
    const int down = row + 1 < size_ ? h[idx + static_cast<std::size_t>(size_)] : 0;
    const int left = col > 0 ? h[idx - 1] : 0;
    const int right = col + 1 < size_ ? h[idx + 1] : 0;

    return value - up > theta_ || value - down > theta_ ||
           value - left > theta_ || value - right > theta_;
}

bool sand_pile::is_stable() const
{
    for (int row = 0; row < size_; ++row)
    {
        for (int col = 0; col < size_; ++col)
        {
            if (unstable_at(row, col))
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
std::array<long long, kDirectionCount> sand_pile::collapse_plan(int row, int col) const
{
    const auto& h = heights_;
    const std::size_t idx = cell_index(row, col);

    std::array<long long, kDirectionCount> levels{};
    std::array<bool, kDirectionCount> inside{};

    if (row > 0)
    {
        levels[kUp] = h[idx - static_cast<std::size_t>(size_)];
        inside[kUp] = true;
    }
    if (col + 1 < size_)
    {
        levels[kRight] = h[idx + 1];
        inside[kRight] = true;
    }
    if (row + 1 < size_)
    {
        levels[kDown] = h[idx + static_cast<std::size_t>(size_)];
        inside[kDown] = true;
    }
    if (col > 0)
    {
        levels[kLeft] = h[idx - 1];
        inside[kLeft] = true;
    }

    long long height = h[idx];
    std::array<long long, kDirectionCount> alloc{};

    while (height - *std::min_element(levels.begin(), levels.end()) > theta_)
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
long long sand_pile::relax()
{
    const std::size_t cell_count = heights_.size();
    long long avalanche_size = 0;
    std::vector<long long> delta(cell_count);
    std::vector<std::size_t> unstable_cells;

    while (true)
    {
        unstable_cells.clear();
        for (int row = 0; row < size_; ++row)
        {
            for (int col = 0; col < size_; ++col)
            {
                if (unstable_at(row, col))
                {
                    unstable_cells.push_back(cell_index(row, col));
                }
            }
        }

        if (unstable_cells.empty())
        {
            break;
        }

        std::fill(delta.begin(), delta.end(), 0);

        // 所有计划都基于本轮开始时的同一份旧快照 heights_。
        for (const std::size_t idx : unstable_cells)
        {
            const int row = static_cast<int>(idx / static_cast<std::size_t>(size_));
            const int col = static_cast<int>(idx % static_cast<std::size_t>(size_));
            const auto alloc = collapse_plan(row, col);

            long long sent = 0;
            for (const long long grains : alloc)
            {
                sent += grains;
            }

            avalanche_size += sent;
            delta[idx] -= sent;

            if (row > 0)
            {
                delta[idx - static_cast<std::size_t>(size_)] += alloc[kUp];
            }
            if (col + 1 < size_)
            {
                delta[idx + 1] += alloc[kRight];
            }
            if (row + 1 < size_)
            {
                delta[idx + static_cast<std::size_t>(size_)] += alloc[kDown];
            }
            if (col > 0)
            {
                delta[idx - 1] += alloc[kLeft];
            }
        }

        // 统一提交本轮所有变化。
        for (std::size_t i = 0; i < cell_count; ++i)
        {
            heights_[i] += static_cast<int>(delta[i]);
        }
    }

    return avalanche_size;
}

long long sand_pile::update(int row, int col)
{
    if (!in_bounds(row, col))
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    // README 第 4 节硬性约束：上一轮松弛结束前禁止落沙。
    if (!is_stable())
    {
        return SAND_ERR_NOT_STABLE;
    }

    ++heights_[cell_index(row, col)];
    return relax();
}

extern "C" SAND_API sand_pile* sand_create(int size, int theta)
{
    if (size <= 0 || theta < 0)
    {
        return nullptr;
    }

    try
    {
        return new sand_pile(size, theta);
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
    if (sand == nullptr)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    sand->reset();
    return 0;
}

extern "C" SAND_API int sand_size(const sand_pile* sand)
{
    if (sand == nullptr)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    return sand->size();
}

extern "C" SAND_API int sand_theta(const sand_pile* sand)
{
    if (sand == nullptr)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    return sand->theta();
}

extern "C" SAND_API int sand_set_theta(sand_pile* sand, int theta)
{
    if (sand == nullptr || !sand->set_theta(theta))
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    return 0;
}

extern "C" SAND_API int sand_get_height(const sand_pile* sand, int row, int col)
{
    if (sand == nullptr)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    return sand->get_height(row, col);
}

extern "C" SAND_API int sand_set_height(sand_pile* sand, int row, int col, int height)
{
    if (sand == nullptr || !sand->set_height(row, col, height))
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    return 0;
}

extern "C" SAND_API int sand_is_stable(const sand_pile* sand)
{
    if (sand == nullptr)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    return sand->is_stable() ? 1 : 0;
}

extern "C" SAND_API long long sand_relax(sand_pile* sand)
{
    if (sand == nullptr)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    return sand->relax();
}

extern "C" SAND_API long long sand_relax_grid(int* heights, int size, int theta)
{
    if (heights == nullptr || size <= 0 || theta < 0)
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
        sand_pile pile(size, theta);
        if (!pile.load_heights(heights, cell_count))
        {
            return SAND_ERR_INVALID_ARGUMENT;
        }

        const long long avalanche_size = pile.relax();
        pile.store_heights(heights);
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
    if (sand == nullptr)
    {
        return SAND_ERR_INVALID_ARGUMENT;
    }

    return sand->update(row, col);
}

extern "C" SAND_API long long sand_update_grid(int* heights, int size, int theta, int row, int col)
{
    if (heights == nullptr || size <= 0 || theta < 0 ||
        !in_bounds(size, row, col))
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
        sand_pile pile(size, theta);
        if (!pile.load_heights(heights, cell_count))
        {
            return SAND_ERR_INVALID_ARGUMENT;
        }

        const long long result = pile.update(row, col);
        if (result >= 0)
        {
            pile.store_heights(heights);
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

extern "C" SAND_API long long sand_drop_grain(sand_pile* sand, int row, int col)
{
    return sand_update(sand, row, col);
}

extern "C" SAND_API long long sand_update_height(sand_pile* sand, int row, int col)
{
    return sand_update(sand, row, col);
}

extern "C" SAND_API long long update_sand_height(int* heights, int size, int theta, int row, int col)
{
    return sand_update_grid(heights, size, theta, row, col);
}

extern "C" SAND_API long long update_height(int* heights, int size, int theta, int row, int col)
{
    return sand_update_grid(heights, size, theta, row, col);
}
