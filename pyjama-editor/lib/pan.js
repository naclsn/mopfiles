/*-*/;(function(){
  class _PanSide {
    constructor(element, parent) {
      this.element = element;
      this.parent = parent;
      this.child = null;
      this.data = undefined;
    }
    toJSON() {
      return {
        child: this.child ? this.child.toJSON("child") : undefined,
        data: this.data && this.data.toJSON ? this.data.toJSON("data") : this.data,
      };
    }
    attach(child) {
      if (!(child instanceof Pan)) throw TypeError("Expected a instanceof Pan, got " + typeof child);
      this.child = child;
    }
    update() {
      if (this.child) this.child.update();
    }
    depth() {
      return this.child ? this.child.depth() : 0;
    }
    balance() {
      return this.child ? this.child.balance() : 1;
    }
    forEach(callbackfn, depthFirst) {
      if (this.child) this.child.forEach(callbackfn, depthFirst);
    }
    split(direction, thickness, onResized) {
      var elm = this.element.children[0]
        , pan = new Pan(this, direction, thickness, onResized);
      if (elm) pan.first.element.appendChild(elm);
      pan.first.data = this.data;
      this.data = undefined;
      this.attach(pan);
      return pan;
    }
    getTwin() {
      return this.parent.first === this ? this.parent.second : this.parent.first;
    }
    discard() {
      var other = this.getTwin()
        , elm = other.parent.parent.element;
      other.parent.parent.child = other.child;
      other.parent.parent.data = other.data;
      other.parent.parent.element.innerHTML = "";
      other.parent.parent.element.appendChild(other.element.children[0]);
      other.element = elm;
      other.parent = other.parent.parent;
      other.update();
    }
  }
  window.Pan = class Pan {
    constructor(parent, direction, thickness, onResized) {
      function _makeDiv(classes) {
        var it = document.createElement('div');
        it.setAttribute('class', classes);
        return it;
      }
      var dir = direction || Pan.Horizontal
        , elm =
      this.element = _makeDiv('pan-splitter pan-splitter-' + dir);
      this.first = new _PanSide(elm.appendChild(_makeDiv('pan-first')), this, "first");
      this.separator = elm.appendChild(_makeDiv('pan-separator pan-separator-' + dir));
      this.second = new _PanSide(elm.appendChild(_makeDiv('pan-second')), this, "second");
      this.parent = null;
      this.direction = elm.style.flexDirection = dir;
      this.ratio = .5;
      this.separator.style[Pan.Vertical === this.direction ? 'height' : 'width'] = (this.thickness = thickness || 10) + "px";
      if (parent) {
        if (parent instanceof _PanSide) {
          parent.attach(this);
          this.parent = parent;
        } else this.parent = { element: parent };
        this.parent.element.appendChild(elm);
        this.enable(onResized);
        this.update(.5);
      }
      this._id = (Math.random()*1000)<<0;
    }
    toJSON() {
      return {
        first: this.first.toJSON("first"),
        second: this.second.toJSON("second"),
        direction: Pan.Vertical === this.direction ? "Vertical" : "Horizontal",
        ratio: this.ratio,
      };
    }
    enable(onResized) {
      var pan = this
        , sep = pan.separator
        , fir_ps = pan.first
        , sec_ps = pan.second
        , fir = fir_ps.element
        , sec = sec_ps.element
        , dir = pan.direction
        , hth = pan.thickness / 2
        , md;
      pan.enabled = true;
      sep.onmousedown =
      function _onMouseDown(e) {
        md = {
          e: e,
          offsetLeft: sep.offsetLeft,
          offsetTop: sep.offsetTop,
          firstWidth: fir.offsetWidth,
          secondWidth: sec.offsetWidth,
          firstHeight: fir.offsetHeight,
          secondHeight: sec.offsetHeight,
        };
        var doc = document;
        doc.onmousemove = _onMouseMove;
        doc.onmouseup = function() {
          doc.onmousemove = doc.onmouseup = null;
          if (onResized) onResized(pan);
        };
      };
      function _onMouseMove(e) {
        var delta = { x: e.clientX-md.e.clientX, y: e.clientY-md.e.clientY }
          , a, b;
        if (Pan.Horizontal === dir) {
          delta.x = Math.min(Math.max(delta.x, -md.firstWidth), md.secondWidth);
          // sep.style.left = md.offsetLeft + delta.x - 2*hth + "px";
          fir.style.width = (a = md.firstWidth + delta.x) /*- hth*/ + "px";
          sec.style.width = (b = md.secondWidth - delta.x) /*- hth*/ + "px";
        } else if (Pan.Vertical === dir) {
          delta.y = Math.min(Math.max(delta.y, -md.firstHeight), md.secondHeight);
          // sep.style.top = md.offsetTop + delta.y - 2*hth + "px";
          fir.style.height = (a = md.firstHeight + delta.y) /*- hth*/ + "px";
          sec.style.height = (b = md.secondHeight - delta.y) /*- hth*/ + "px";
        }
        pan.ratio = a / (a+b);
        fir_ps.update();
        sec_ps.update();
      }
    }
    disable() {
      this.enabled = false;
      this.element.onmousedown = null;
    }
    update(ratio) {
      var pan = this
        , sep = pan.separator
        , fir_ps = pan.first
        , sec_ps = pan.second
        , fir = fir_ps.element
        , sec = sec_ps.element
        , dir = pan.direction
        , hth = pan.thickness / 2
        , r = ratio || pan.ratio
        , t;
      if (Pan.Horizontal === dir) {
        t = pan.element.offsetWidth;
        // sep.style.left = sep.offsetLeft * r - 2*hth + "px";
        fir.style.width = t * r /*- hth*/ + "px";
        sec.style.width = t * (1-r) /*- hth*/ + "px";
      } else if (Pan.Vertical === dir) {
        t = pan.element.offsetHeight;
        // sep.style.top = sep.offsetTop * r - 2*hth + "px";
        fir.style.height = t * r /*- hth*/ + "px";
        sec.style.height = t * (1-r) /*- hth*/ + "px";
      }
      pan.ratio = r;
      fir_ps.update();
      sec_ps.update();
    }
    depth() {
      var a = this.first.depth(), b = this.second.depth();
      return (a < b ? b : a) + 1;
    }
    balance() {
      var a = this.first.balance()
        , b = this.second.balance()
        , r = a + b;
      this.update(a / r);
      return r;
    }
    forEach(callbackfn, depthFirst) {
      if (!depthFirst) callbackfn(this);
      this.first.forEach(callbackfn, depthFirst);
      this.second.forEach(callbackfn, depthFirst);
      if (depthFirst) callbackfn(this);
    }
    destroy() {
      this.element.innerHTML = "";
      delete this.first;
      delete this.second;
      if (this.parent.child) this.parent.child = null;
      if (this.parent.element) this.parent.element.innerHTML = "";
    }
  };
  Pan.Horizontal = "row";
  Pan.Vertical = "column";
  Pan.parse = function(serialized, onNodeCreated, rootParent, thickness, onResized) {
    function _walkTree(here, onto) {
      var niw = new Pan(onto, Pan[here.direction], thickness, onResized);
      niw.first.data = here.first.data;
      niw.second.data = here.second.data;
      if (here.first.child) _walkTree(here.first.child, niw.first);
      if (here.second.child) _walkTree(here.second.child, niw.second);
      onNodeCreated(niw);
      niw.update(here.ratio);
      return niw;
    }
    return _walkTree(JSON.parse(serialized), rootParent);
  };
  document.head.appendChild(document.createElement('style')).textContent = `
    .pan-splitter {
      height: 100%;
      display: flex;
    }
    .pan-separator {
      background-repeat: no-repeat;
      background-position: center;
      -moz-user-select: none;
      -ms-user-select: none;
      user-select: none;
    }
    .pan-separator-row {
      cursor: ew-resize;
      background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='10' height='30'><path d='M2 0 v30 M5 0 v30 M8 0 v30' fill='none' stroke='black'/></svg>");
      /*width: 10px;*/
      height: 100%;
    }
    .pan-separator-column {
      cursor: ns-resize;
      background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='30' height='10'><path d='M0 2 h30 M0 5 h30 M0 8 h30' fill='none' stroke='black'/></svg>");
      /*height: 10px;*/
    }
    .pan-first {
      height: 100%;
      min-width: 10px;
      min-height: 10px;
    }
    .pan-second {
      height: 100%;
      min-width: 10px;
      min-height: 10px;
    }
  `;
}());
