#pragma once
#include <array>
#include <cstddef>

namespace qin
{
// Each oscillator has one FM source: -1 is Off, 0..2 are oscillators, 3 is Noise.
// A cycle uses the preceding sample on every edge inside that cycle. Edges
// outside cycles retain same-sample precedence, so modulated sources can stack.
struct FmRouting
{
    static constexpr int count=3;
    std::array<int,count> order {0,1,2};
    std::array<bool,count> delayedEdge {};

    static FmRouting calculate(const std::array<int,count>& source) noexcept
    {
        FmRouting result;
        for(int start=0;start<count;++start)
        {
            std::array<bool,count> seen {};
            int cursor=start;
            while(cursor>=0 && cursor<count && !seen[size_t(cursor)])
            { seen[size_t(cursor)]=true; cursor=source[size_t(cursor)]; }
            if(cursor<0 || cursor>=count) continue;
            const auto cycleStart=cursor;
            do
            {
                result.delayedEdge[size_t(cursor)]=true;
                cursor=source[size_t(cursor)];
            } while(cursor!=cycleStart);
        }
        std::array<int,count> incoming {};
        for(int target=0;target<count;++target)
            if(source[size_t(target)]>=0 && source[size_t(target)]<count
               && !result.delayedEdge[size_t(target)])
                ++incoming[size_t(target)];
        std::array<bool,count> emitted {};
        for(int position=0;position<count;++position)
        {
            int selected=-1;
            for(int osc=0;osc<count;++osc)
                if(!emitted[size_t(osc)] && incoming[size_t(osc)]==0)
                { selected=osc; break; }
            if(selected<0) return result; // Defensive: cycle edges were removed.
            result.order[size_t(position)]=selected;
            emitted[size_t(selected)]=true;
            for(int target=0;target<count;++target)
                if(source[size_t(target)]==selected && !result.delayedEdge[size_t(target)])
                    --incoming[size_t(target)];
        }
        return result;
    }
};
}
