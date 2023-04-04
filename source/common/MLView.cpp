
// mlvg: GUI library for madronalib apps and plugins
// Copyright (C) 2019-2022 Madrona Labs LLC
// This software is provided 'as-is', without any express or implied warranty.
// See LICENSE.txt for details.


#include "MLView.h"
#include "MLDSPProjections.h"


using namespace ml;

// TEMP
auto red = nvgRGBA(255, 20, 80, 255);
auto yellow = nvgRGBA(255, 220, 80, 255);

View::View(ml::Collection< Widget > t, WithValues p) : Widget(p), _widgets(t)
{
  
}

void View::setDirty(bool d)
{
  forEach< Widget >
  (_widgets, [&](Widget& w)
   {
    w.setDirty(d);
  }
   );
  _dirty = d;
}

// slow reverse lookup of Widget name, for debugging only!
Path View::_widgetPointerToName(Widget* wPtr)
{
  Path p, q;
  forEachChild< Widget >
  (_widgets, [&](Widget& w)
   {
    if(wPtr == &w) {q = p;}
    }, &p
   );
  return q;
}

// Process GUI events in this View and keep track of the stillDown widget.
// TODO allow scale and offset of view
MessageList View::processGUIEvent(const GUICoordinates& gc, GUIEvent e)
{
  constexpr bool kDebug{false};
  constexpr float kDragRepositionDistance{1.0f};
  MessageList r;
  
  auto wvec = findWidgetsForEvent(e);
  if(wvec.size() > 0)
  {
    std::sort(wvec.begin(), wvec.end(), [&](Widget* a, Widget* b){
      return (a->getProperty("z").getFloatValue() < b->getProperty("z").getFloatValue());
    } );
  }
  
  if(_stillDownWidget)
  {
    auto messagesFromWidget = (_stillDownWidget->processGUIEvent(gc, e));
    
    // DEBUG
    if(kDebug)
    {
      if (messagesFromWidget.size() > 0)
      {
        auto widgetName = _widgetPointerToName(_stillDownWidget);
        std::cout << e.type << " from (stilldown) " << widgetName << "\n";
      }
    }
    
    if(e.type == "up")
    {
      _stillDownWidget = nullptr;
    }
    r = messagesFromWidget;
  }
  else
  {
    // iterate until some widget replies with one or more messages.
    
    for(Widget* w : wvec)
    {
      auto messagesFromWidget = w->processGUIEvent(gc, e);
      
      if (messagesFromWidget.size() > 0)
      {
        if(kDebug)
        {
          auto widgetName = _widgetPointerToName(w);
          std::cout << e.type << " from " << widgetName << "\n";
        }
        
        // only set the stilldownWidget if the widget has returned some message.
        // otherwise the event passes thru.
        if(e.type == "down")
        {
          _stillDownWidget = w;
        }
        
        // because we break here, widgets block events from widgets behind
        // them, which is intentional.
        r = messagesFromWidget;
        break;
      }
    }
  }

  return r;
}

MessageList View::animate(int elapsedTimeInMs, ml::DrawContext dc)
{
  MessageList v;
  
  // TEST
  testCounter += elapsedTimeInMs;
  if(testCounter > 1000)
  {
    secsCounter++;
    testCounter -= 1000;
  }
  
  forEachChild< Widget >
  (_widgets,
   [&](Widget& w) { v.append(w.animate(elapsedTimeInMs, dc)); }
   );
  
  return v;
}

void View::draw(ml::DrawContext dc)
{
  _frameCounter++;

  NativeDrawContext* nvg = getNativeContext(dc);
  Rect nativeBounds = getLocalBounds(dc, *this);
  Vec2 nativeCenter = getCenter(nativeBounds);
  
  // TODO before scaling, is this widget or any children dirty?
  
  // we save the context before every Widget is drawn,
  // so no need to save it here when we change the transform
  if(hasProperty("scale"))
  {
    float s = getFloatProperty("scale");
    nvgTranslate(nvg, nativeCenter);
    nvgScale(nvg, s, s);
    nvgTranslate(nvg, -nativeCenter);
  }
  
  if(hasProperty("position"))
  {
    nvgTranslate(nvg, dc.coords.gridToPixel(matrixToVec2(getMatrixProperty("position"))));
  }
  
  // if the View itself is dirty, all the Widgets in it must be redrawn.
  // otherwise, only the dirty Widgets need to be redrawn.
  if(_dirty)
  {
    if(getBoolPropertyWithDefault("draw_background", true))
    {
      drawBackground(dc, nativeBounds);
    }
    drawAllWidgets(dc);
  }
  else
  {
    drawDirtyWidgets(dc);
  }
}

std::vector< Widget* > View::findWidgetsForEvent(const GUIEvent& e)
{
  std::vector< Widget* > widgetsForEvent;
  
  forEachChild< Widget >
  (_widgets, [&](Widget& w)
   {
    if(w.getProperty("visible").getBoolValueWithDefault(true))
    {
      if(w.hasProperty("bounds"))
      {
        //std::cout << "find: " << e.position << " in " << p << " at " <<
        //getBounds(w) << " z=" << w.getFloatProperty("z") << " (" << within(e.position, getBounds(w)) << ")\n";
        
        if(within(e.position, w.getBounds()))
        {
          widgetsForEvent.push_back(&w);
        }
      }
    }
  }
   );
  
  return widgetsForEvent;
}

// draw widget in the current View context.
// each widget is drawn in View coordinates, with the origin at its top left.
// if widget is a view, it may draw sub-widgets.
void View::drawWidget(const ml::DrawContext& dc, Widget* w)
{
  NativeDrawContext* nvg = getNativeContext(dc);
  Rect widgetBounds = getPixelBounds(dc, *w);
  
  nvgSave(nvg);
  nvgIntersectScissor(nvg, widgetBounds);
  nvgTranslate(nvg, getTopLeft(widgetBounds));
  w->draw(dc);
  w->setDirty(false);
  nvgRestore(nvg);
  
  bool kShowWidgetBounds = dc.pProperties->getBoolPropertyWithDefault("draw_widget_bounds", false);
  if(kShowWidgetBounds)
  {
    nvgBeginPath(nvg);
    nvgStrokeColor(nvg, rgba(0, 1, 0, 1.0));
    nvgStrokeWidth(nvg, 1);
    nvgRect(nvg, widgetBounds + ml::Rect{0.5, 0.5, 0., 0.});
    nvgStroke(nvg);
  }
  
  bool kShowDirtyWidgets = dc.pProperties->getBoolPropertyWithDefault("draw_dirty_widgets", false);
  if(kShowDirtyWidgets)
  {
    // DEBUG
    size_t x = (_frameCounter&0x3F);
    float omega = x/64.f;
    static auto fb = projections::linear({-1, 1}, {0.4, 0.8});
    float b = fb(sinf(omega*ml::kTwoPi));
    
    auto flickerColor = rgba(b, b, 0, 1.);
    
    nvgBeginPath(nvg);
    nvgStrokeColor(nvg, flickerColor);
    nvgStrokeWidth(nvg, 4);
    nvgRect(nvg, widgetBounds);
    nvgStroke(nvg);
  }
}

// draw background widget in the current View context.

void View::drawBackgroundWidget(const ml::DrawContext& dc, Widget* w)
{
  NativeDrawContext* nvg = getNativeContext(dc);
  Rect widgetBounds = getPixelBounds(dc, *w);
  
  nvgSave(nvg);
  nvgTranslate(nvg, getTopLeft(widgetBounds));
  w->draw(dc);
  nvgRestore(nvg);
}


void View::drawAllWidgets(ml::DrawContext dc)
{
  std::vector< Widget* > visibleWidgets;
  
  forEachChild< Widget >
  (_widgets, [&](Widget& w)
   {
    // visibleWidgets could be written as a SubCollection with filters, not tests and iterating
    bool visible = w.getBoolProperty("visible");
    if((visible)&&(w.hasProperty("bounds")))
    {
      // TODO if bounds intersects view bounds
      visibleWidgets.push_back(&w);
    }
  });
  
  // draw all widgets in z order.
  std::sort(visibleWidgets.begin(), visibleWidgets.end(), [&](Widget* a, Widget* b){ return (a->getProperty("z").getFloatValue() > b->getProperty("z").getFloatValue());} );
  
  for(auto w : visibleWidgets)
  {
    drawWidget(dc, w);
  }
}

struct WidgetGroup
{
  Rect bounds;
  std::vector< Widget* > widgets;
  
  WidgetGroup(Widget& w)
  {
    bounds = getCurrentAndPreviousBounds(w);
    widgets.push_back(&w);
  }
  ~WidgetGroup() = default;
  
  void add(Widget* w)
  {
    bounds = rectEnclosing(bounds, w->getBounds());
    widgets.push_back(w);
  }
  
  void mergeWithGroup(const WidgetGroup& wg2)
  {
    for(Widget* w2 : wg2.widgets)
    {
      widgets.push_back(w2);
    }
    bounds = rectEnclosing(bounds, wg2.bounds);
  }
};

struct WidgetGroupList
{
//  std::vector< std::unique_ptr< WidgetGroup > > groups;
  std::vector< WidgetGroup > groups;

  /*
  WidgetGroup& add(WidgetGroup wg)
  {
    groups.emplace_back(wg);
    return groups->back();
  }*/
};

void View::drawDirtyWidgets(ml::DrawContext dc)
{
  NativeDrawContext* nvg = getNativeContext(dc);
  
  static int frameCounter{};
  frameCounter++;
  
 // std::vector< Widget* > visibleWidgets;
  
  WidgetGroupList widgetGroups;
  
  
  /*
  // we use forEachChild to draw only the visible widgets at this level of the tree.
  // any widgets at lower levels will be drawn in subviews.
  forEachChild< Widget >
  (_widgets, [&](Widget& w)
   {
    if(w.getBoolProperty("visible") && w.hasProperty("bounds"))
    {
      // TODO if bounds intersects view bounds
      visibleWidgets.push_back(&w);
    }
  }
   );
  std::sort(visibleWidgets.begin(), visibleWidgets.end(), [&](Widget* a, Widget* b){ return (a->getProperty("z").getFloatValue() > b->getProperty("z").getFloatValue());} );
  */
  
  // instead this visibility check above could be implemented as a SubCollection.
  // auto visibleWidgets = reduce(children(_widgets), [](const Widget& w){ return w.getBoolProperty("visible") && w.hasProperty("bounds"); }
  // auto visibleWidgetsSorted = sort(visibleWidgets, [&](Widget* a, Widget* b){ return (a->getProperty("z").getFloatValue() > b->getProperty("z").getFloatValue());} );
  
  
  // in any order, make list of widgets needing redraw and any widgets overlapping them.
  // keep track of rectangle groups of widgets: the groups grow and "eat" any widgets overlapping the rectangle.
  //
  
  // clear needsDraw flags
  forEachChild< Widget >
  (_widgets, [&](Widget& w)
   { w._needsDraw = false; }
   );
  
  size_t sweepIters{0};
  forEachChild< Widget >
  (_widgets, [&](Widget& w)
   {
      if(w.getBoolProperty("visible") && w.isDirty())
      {
//        WidgetGroup& newGroup = widgetGroups.add(WidgetGroup(w));
            
       
     //   auto newGroupPtr = make_unique<WidgetGroup>(w);
        
     //   widgetGroups.groups.push_back(make_unique<WidgetGroup>(w));

      //  WidgetGroup& newGroup = *newGroupPtr;
        
        WidgetGroup newGroup(w);
        
      //  WidgetGroup newGroup = widgetGroups.add(WidgetGroup(w));
        w._needsDraw = true;
        //int sizeBefore = widgetGroups.groups.size();
        
        
        while(1)
        {
          sweepIters++;
          bool changed{false};
          // if new group overlaps any Widgets w2 that are not marked as
          // needing drawing, add w2 to the group.
          forEachChild< Widget >
          (_widgets, [&](Widget& w2)
           {
            
            // if newGroup intersects visible w2, add to group
            if(w2.getBoolProperty("visible") && intersectRects(newGroup.bounds, w2.getBounds()))
            {
              if(!w2._needsDraw)
              {
                newGroup.add(&w2);
                w2._needsDraw = true;
                changed = true;
              }
            }
          }
           );
          
          // if new group overlaps any other group wg2, merge wg2 into new group
          // and delete wg2 from list
          for(auto it = widgetGroups.groups.begin(); it != widgetGroups.groups.end(); )
          {
            WidgetGroup& wg2 = *it;
            if(intersectRects(newGroup.bounds, wg2.bounds))
            {
              newGroup.mergeWithGroup(wg2);
              it = widgetGroups.groups.erase(it);
              changed = true;
            }
            else
            {
              it++;
            }
          }
          
          if(!changed) break;
        }
        
        // add new group to the group list
        widgetGroups.groups.push_back(newGroup);
      }
   }
   );
  
  // TEST
  
  for(auto& wg : widgetGroups.groups)
  {
    for(auto w : wg.widgets)
    {
      ml::Rect widgetBounds = w->getBounds();
      auto nativeBounds = dc.coords.gridToPixel(widgetBounds);
      //nvgIntersectScissor(nvg, nativeBounds);
      
      drawBackground(dc, nativeBounds);
      drawWidget(dc, w);
    }
    
    // TEMP
    {
      Rect widgetBounds = roundToInt(dc.coords.gridToPixel(wg.bounds));
      
      nvgBeginPath(nvg);
      nvgStrokeColor(nvg, colors::red);
      nvgStrokeWidth(nvg, 3);
      nvgRect(nvg, widgetBounds + ml::Rect{0.5, 0.5, 0., 0.});
      nvgStroke(nvg);
    }
  }
  
  if(widgetGroups.groups.size()  > 0)
  {
    std::cout << "drawing: " << sweepIters << " iters, " << widgetGroups.groups.size() << " groups\n";
  }
  
  // group and merge:
  
  // if a widget w is dirty,
  // get prev + current bounds rect wb
  // for each rectangle-group a,
  // if wb overlaps the rectangle-group a, add w to the rectangle-group
  // calculate new group bounds rect ab
  // for each other rectangle group b,
  // if ab overlaps b, merge b into a
  
  
  /*
  forEachChild< Widget >
  (_widgets, [&](Widget& w)
   {
    if(w.getBoolProperty("visible") && w.hasProperty("bounds")) // TODO clean
    {
      // TODO if bounds intersects view bounds
      visibleWidgets.push_back(&w);
    }
    
    if(w.isDirty())
    {
    }
  }
   );
  
  
  */
  
  
  
  // in z sorted order from back to front:

  
  /*
  // for each widget needing drawing, mark as dirty all widgets intersecting its bounds
  for(auto it = visibleWidgets.begin(); it != visibleWidgets.end(); ++it)
  {
    Widget* w1 = *it;
    if(w1->isDirty())
    {
      ml::Rect widget1Bounds = w1->getBounds();
      if(w1->getProperty("previous_bounds"))
      {
        ml::Rect widget1PreviousBounds = matrixToRect(w1->getProperty("previous_bounds").getMatrixValue());
        widget1Bounds = rectEnclosing(widget1Bounds, widget1PreviousBounds);
      }
      for(auto it2 = visibleWidgets.begin(); it2 != visibleWidgets.end(); ++it2)
      {
        Widget* w2 = *it2;
        if(w1 != w2)
        {
          if(!w2->isDirty())
          {
            if(intersectRects(widget1Bounds, w2->getBounds()))
            {
              w2->setDirty(true);
            }
          }
        }
      }
    }
  }
  
  // collect previous and current bounds of each dirty widget. Previous
  // bounds are needed in case widget has moved.
  std::vector< Rect > dirtyRects;
  for(auto w : visibleWidgets)
  {
    if(w->isDirty())
    {
      if(w->getProperty("previous_bounds"))
      {
        dirtyRects.push_back(matrixToRect(w->getProperty("previous_bounds").getMatrixValue()));
      }
      dirtyRects.push_back(w->getBounds());
    }
  }
  
  */
  
  
  
  /*
  // fill dirty rects with background.
  // TODO we want a non-rectangular scissor region but don't have it in nanovg so we make a bunch
  // of drawBackground calls with rects. instead try one big rect.
  if(getBoolPropertyWithDefault("draw_background", true))
  {
    for(auto rect : dirtyRects)
    {
      nvgSave(nvg);
      auto nativeRect = dc.coords.gridToPixel(rect);
      nativeRect = grow(nativeRect, 2); // MLTEST workaround for slop
      nvgIntersectScissor(nvg, nativeRect);
      
      drawBackground(dc, nativeRect);
      nvgRestore(nvg);
    }
  }
  */
  
  /*
  // draw each dirty widget.
  for(auto w : visibleWidgets)
  {
    if(w->isDirty())
    {
      drawWidget(dc, w);
    }
  }
   */
  
  
}

// draw a rectangle of the background.
void View::drawBackground(DrawContext dc, ml::Rect nativeRect)
{
  NativeDrawContext* nvg = getNativeContext(dc);
  //auto white = nvgRGBAf(1, 1, 1, 0.25);
  auto bgColorA = nvgRGBA(131, 162, 199, 255);
  auto bgColorB = nvgRGBA(115, 149, 185, 255);
  int u = dc.coords.gridSizeInPixels;
  
  // MLTEST
  // std::cout << "view_size: " << dc.coords.viewSizeInPixels << "\n";
  
  // get image or gradient
  NVGpaint paintPattern;
  Resource* pr = getResource(dc, "background");
  if(pr)
  {
    // TODO better cache image handles
    const int* pImgHandle = reinterpret_cast<const int *>(pr->data());
    int bgh = pImgHandle[0];
    int backgroundHandle = bgh;
    
    //  int imgw, imgh;
    //  nvgImageSize(nvg, backgroundHandle, &imgw, &imgh);
    paintPattern = nvgImagePattern(nvg, -u, -u, dc.coords.viewSizeInPixels.x() + u, dc.coords.viewSizeInPixels.y() + u, 0, backgroundHandle, 1.0f);
  }
  else
  {
    auto bgColor = getColor(dc, "background");
    paintPattern = nvgLinearGradient(nvg, 0, -u, 0, dc.coords.viewSizeInPixels.y() + u, bgColor, bgColor);
  }
  
  // do the drawing
  {
    nvgSave(nvg);
    nvgIntersectScissor(nvg, nativeRect);
    
    // draw image or gradient
    nvgBeginPath(nvg);
    nvgRect(nvg, nativeRect);
    nvgFillPaint(nvg, paintPattern);
    nvgFill(nvg);
    
    // draw background Widgets intersecting rect
    for(const auto& w : _backgroundWidgets)
    {
      if(intersectRects(dc.coords.gridToPixel(w->getBounds()), nativeRect))
      {
        drawBackgroundWidget(dc, w.get());
      }
    };
    
    bool drawGrid = dc.pProperties->getBoolPropertyWithDefault("draw_background_grid", false);
    
    if(drawGrid)
    {
      auto markColor = multiplyAlpha(getColor(dc, "mark"), 0.125f);
      
      const int gx = std::ceilf(getFloatProperty("grid_units_x"));
      const int gy = std::ceilf(getFloatProperty("grid_units_y"));
      
      nvgStrokeColor(nvg, markColor);
      nvgStrokeWidth(nvg, 1.0f*dc.coords.displayScale);
      
      nvgBeginPath(nvg);
      for(int i=0; i <= gx; ++i)
      {
        Vec2 a = dc.coords.gridToPixel(Vec2(i, 0));
        Vec2 b = dc.coords.gridToPixel(Vec2(i, gy));
        nvgMoveTo(nvg, a.x(), a.y());
        nvgLineTo(nvg, b.x(), b.y());
      }
      for(int j=0; j <= gy; ++j)
      {
        Vec2 a = dc.coords.gridToPixel(Vec2(0, j));
        Vec2 b = dc.coords.gridToPixel(Vec2(gx, j));
        nvgMoveTo(nvg, a.x(), a.y());
        nvgLineTo(nvg, b.x(), b.y());
      }
      nvgStroke(nvg);
    }
    nvgRestore(nvg);
  }
}


