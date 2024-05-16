/*
 * Copyright (c) 2020 - 2024 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _TVG_CANVAS_H_
#define _TVG_CANVAS_H_

#include "tvgPaint.h"


struct Canvas::Impl
{
    enum Status : uint8_t {Synced = 0, Updating, Drawing};

    list<Paint*> paints;
    RenderMethod* renderer;
    Status status = Status::Synced;

    bool refresh = false;   //if all paints should be updated by force.

    Impl(RenderMethod* pRenderer) : renderer(pRenderer)
    {
        renderer->ref();
    }

    ~Impl()
    {
        //make it sure any deffered jobs
        if (renderer) renderer->sync();

        clearPaints();

        if (renderer && (renderer->unref() == 0)) delete(renderer);
    }

    void clearPaints()
    {
        for (auto paint : paints) {
            if (P(paint)->unref() == 0) delete(paint);
        }
        paints.clear();
    }

    Result push(unique_ptr<Paint> paint)
    {
        //You can not push paints during rendering.
        if (status == Status::Drawing) return Result::InsufficientCondition;

        auto p = paint.release();
        if (!p) return Result::MemoryCorruption;
        PP(p)->ref();
        paints.push_back(p);

        return update(p, true);
    }

    Result clear(bool paints, bool buffer)
    {
        if (status == Status::Drawing) return Result::InsufficientCondition;

        //Clear render target
        if (buffer) {
            if (!renderer || !renderer->clear()) return Result::InsufficientCondition;
        }

        if (paints) clearPaints();

        return Result::Success;
    }

    void needRefresh()
    {
        refresh = true;
    }

    Result update(Paint* paint, bool force)
    {
        if (paints.empty() || status == Status::Drawing) return Result::InsufficientCondition;

        Array<RenderData> clips;
        auto flag = RenderUpdateFlag::None;
        if (refresh || force) flag = RenderUpdateFlag::All;

        if (paint) {
            paint->pImpl->update(renderer, nullptr, clips, 255, flag);
        } else {
            for (auto paint : paints) {
                paint->pImpl->update(renderer, nullptr, clips, 255, flag);
            }
            refresh = false;
        }
        status = Status::Updating;
        return Result::Success;
    }

    Result draw()
    {
        if (status == Status::Drawing || paints.empty() || !renderer->preRender()) return Result::InsufficientCondition;

        bool rendered = false;
        for (auto paint : paints) {
            if (paint->pImpl->render(renderer)) rendered = true;
        }

        if (!rendered || !renderer->postRender()) return Result::InsufficientCondition;

        status = Status::Drawing;
        return Result::Success;
    }

    Result sync()
    {
        if (status == Status::Synced) return Result::InsufficientCondition;

        if (renderer->sync()) {
            status = Status::Synced;
            return Result::Success;
        }

        return Result::InsufficientCondition;
    }
};

#endif /* _TVG_CANVAS_H_ */
