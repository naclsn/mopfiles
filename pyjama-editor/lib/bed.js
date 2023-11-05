/*-*/;(function(){
  window.Bed = class Bed {
    constructor(element, makeEditor) {
      var ed;
      function _makeDiv(classes) {
        var it = document.createElement('div');
        it.setAttribute('class', classes);
        return it;
      }
      this.base = element.appendChild(_makeDiv('bed-base'));
      this.menu = this.base.appendChild(_makeDiv('bed-menu'));
      this.editor = makeEditor(ed = this.base.appendChild(_makeDiv('bed-editor')));
      ed.style.width = ed.style.height = "100%";
    }
    addMenuItem(tagName) {
      var r = this.menu.appendChild(document.createElement(tagName));
      r.setAttribute('class', 'bed-menu-'+tagName);
      return r;
    }
    removeMenuItem(item) {
      this.menu.removeChild(item);
    }
    addMenuButton(text, onclick, classes) {
      var r = this.addMenuItem('button');
      r.setAttribute('class', (r.getAttribute('class')||'') + ' ' + classes);
      r.textContent = text;
      r.onclick = onclick;
      return r;
    }
    addMenuSelect(options, onchange, defaultOption) {
      function _makeOption(it, k) {
        var opt = r.appendChild(document.createElement('option'));
        opt.textContent = it.text || it.name || it.value || ('string' === typeof it ? it : JSON.stringify(it));
        opt.setAttribute('value', it.value || ('string' === typeof it ? it : k));
      }
      var r = this.addMenuItem('select');
      if (defaultOption) _makeOption(defaultOption, -1);
      options.forEach(_makeOption);
      r.onchange = function(e) { e.option = options[r.value]; onchange(e); };
      return r;
    }
  };
  document.head.appendChild(document.createElement('style')).textContent = `
    .bed-base {
      height: 100%;
      display: flex;
      flex-direction: column;
      overflow: hidden;
    }
    .bed-menu {
      height: 24px;
      display: flex;
      flex-direction: row;
      justify-content: flex-end;
    }
    .bed-menu-button {
      background-repeat: no-repeat;
      background-position: center;
      min-width: 22.8281px;
      padding: 0;
    }
    .bed-icon-close {
      background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='20' height='20'><path d='M4 4 l12 12 M4 16 l12 -12' fill='none' stroke='black'/></svg>");
    }
    .bed-icon-splitv {
      background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='20' height='20'><path d='M10 2 v16 M3 4 v12 h4 v-12 z M13 4 v12 h4 v-12 z' fill='none' stroke='black'/></svg>");
    }
    .bed-icon-splith {
      background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='20' height='20'><path d='M2 10 h16 M4 3 h12 v4 h-12 z M4 13 h12 v4 h-12 z' fill='none' stroke='black'/></svg>");
    }
    .bed-icon-sync-true {
      background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='20' height='20'><path d='M4 9 a5 5 180 0 1 12 0 l2 -5 m-2 5 l-5 -2 M16 11 a5 5 180 0 1 -12 0 l-2 5 m2 -5 l5 2' fill='none' stroke='black'/></svg>");
    }
    .bed-icon-sync-false {
      background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='20' height='20'><path d='M4 9 a5 5 180 0 1 12 0 l2 -5 m-2 5 l-5 -2 M16 11 a5 5 180 0 1 -12 0 l-2 5 m2 -5 l5 2 M2 4 l16 12' fill='none' stroke='black'/></svg>");
    }
  `;
})();
