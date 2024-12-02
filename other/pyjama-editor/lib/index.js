var buffer, views, root;

/*-*/;(function(){
  function _getPref(tag, value) {
    return localStorage.getItem('pyjama-editor;' + tag) || value;
  }

  function _setPref(tag, value) {
    return localStorage.setItem('pyjama-editor;' + tag, value), value;
  }

  function _updateBuffer(editor, event) {
    if ('setValue' === event.origin) return;
    buffer = editor.getValue();
    views.forEach(([_, it]) => it.editor === editor || !it.addedSync || it.editor.setValue(buffer));
    if (!_isSaveTimeoutSet)
      _isSaveTimeoutSet = setTimeout(() => {
        _setPref('buffer', buffer);
        _isSaveTimeoutSet = false;
      }, 5000);
  }

  function _makeInstance(panSide) {
    var bed = new Bed(panSide.element, element => new CodeMirror(element, { value: buffer || "", mode: panSide.data }))
      , r = [panSide, bed];
    if (panSide.data && 'pyjama' !== panSide.data) CodeMirror.autoLoadMode(bed.editor, (CodeMirror.findModeByMIME(panSide.data)||{}).mode);
    bed.editor.setOption('theme', _getPref('theme') || "default");
    bed.editor.on('change', _updateBuffer);
    bed.addMenuSelect(MODES, event => {
      bed.editor.setOption('mode', event.option.mime);
      if ('pyjama' !== event.option.mode)
        CodeMirror.autoLoadMode(bed.editor, event.option.mode);
      bed.addedPanSide.data = event.option.mime;
      _updateViewsPref();
    }, { name: 'Plain Text', mime: 'text/plain', mode: 'null' }).value = panSide.data && MODES.findIndex(it => it.mime === panSide.data) || -1;
    var sync = bed.addedSync = true
      , button = bed.addMenuButton(""/*"toggle sync"*/, () => {
          bed.editor[sync ? 'off' : 'on']('change', _updateBuffer);
          button.setAttribute('class', button.getAttribute('class').replace('bed-icon-sync-'+sync, 'bed-icon-sync-'+!sync));
          button.setAttribute('title', sync ? "Note: this buffer will not be saved!" : "Click to disable buffer synchronization");
          if (sync = bed.addedSync = !sync) bed.editor.setValue(buffer);
        }, 'bed-icon-sync-true');
    button.setAttribute('title', "Click to disable buffer synchronization");
    bed.addMenuButton(""/*"split V"*/, () => _splitView(bed, Pan.Horizontal), 'bed-icon-splitv');
    bed.addMenuButton(""/*"split H"*/, () => _splitView(bed, Pan.Vertical), 'bed-icon-splith');
    if (panSide.parent !== root)
      bed.addMenuButton(""/*"close"*/, () => {
        bed.addedPanSide.discard();
        views.splice(views.indexOf(r), 1);
        _updateViewsPref();
      }, 'bed-icon-close');
    bed.addedPanSide = panSide;
    setTimeout(() => bed.editor.setValue(buffer), 250);
    return r;
  }

  function _splitView(bed, direction) {
    var pan = bed.addedPanSide.split(direction, 10, _updateViewsPref);
    bed.addedPanSide = pan.first;
    views.push(_makeInstance(pan.second));
    _updateViewsPref();
  }

  function _updateThemes(theme) {
    if (!_loadedTheme[theme]) {
      var style = document.head.appendChild(document.createElement('link'));
      style.rel = "stylesheet";
      style.href = "https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.1/theme/"+theme+".min.css";
      _loadedTheme[theme] = true;
    }
    document.getElementById('app').setAttribute('class', ["brightish", "darkish"][THEME_ISH[_selectTheme.value = THEMES.indexOf(theme)]]);
    views.forEach(it => it[1].editor.setOption('theme', theme));
  }

  function _updateViewsPref() {
    _setPref('views', JSON.stringify(root));
  }

  function _loadViews(serialized) {
    var element = document.getElementById('root');
    element.innerHTML = "";
    views = [];
    root = Pan.parse(serialized, node => {
      if (!node.first.child) views.push(_makeInstance(node.first));
      if (!node.second.child) views.push(_makeInstance(node.second));
    }, element, 10, _updateViewsPref);
  }

  var _selectTheme = document.getElementById('select-theme')
    , _loadedTheme = { 'default': true }
    , _isSaveTimeoutSet;

  var THEMES = JSON.parse(`["default","3024-day","3024-night","abbott","abcdef","ambiance-mobile","ambiance","ayu-dark","ayu-mirage","base16-dark","base16-light","bespin","blackboard","cobalt","colorforth","darcula","dracula","duotone-dark","duotone-light","eclipse","elegant","erlang-dark","gruvbox-dark","hopscotch","icecoder","idea","isotope","juejin","lesser-dark","liquibyte","lucario","material-darker","material-ocean","material-palenight","material","mbo","mdn-like","midnight","monokai","moxer","neat","neo","night","nord","oceanic-next","panda-syntax","paraiso-dark","paraiso-light","pastel-on-dark","railscasts","rubyblue","seti","shadowfox","solarized","ssms","the-matrix","tomorrow-night-bright","tomorrow-night-eighties","ttcn","twilight","vibrant-ink","xq-dark","xq-light","yeti","yonce","zenburn"]`)
    , THEME_ISH = "001110111101111111000111101011111111011100111110111110011101110011".split("");

  THEMES.forEach((it, k) => {
    var option = _selectTheme.appendChild(document.createElement('option'));
    option.text = it;
    option.value = k;
  });

  _selectTheme.onchange = () => {
    if (THEMES[_selectTheme.value])
      _updateThemes(_setPref('theme', THEMES[_selectTheme.value]));
  }

  CodeMirror.defineSimpleMode("pyjama", {
    start: [
      { regex: /".*?(?:"|$)/, token: 'string' },
      { regex: /-?\d+/, token: 'number' },
      { regex: /(?:no|cmp|add|sub|mul|div|mod|pow|sum|prd|max|min|v|s|l|span|copy|pick|stow|find|size|hold|drop|does)_/, token: 'keyword' },
      { regex: /(?:program|voila)_/, token: 'meta' },
      { regex: /\(/, indent: true },
      { regex: /\)/, dedent: true },
    ]
  });

  var MODES = Array.from(CodeMirror.modeInfo);
  MODES.push({ name: 'Pyjama', mime: 'pyjama', mode: 'pyjama' });
  MODES = MODES.sort((a, b) => a.name.localeCompare(b.name));

  CodeMirror.modeURL = "https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.1/mode/%N/%N.min.js";

  window.addEventListener('resize', () => root.update());

  window.resetViews = resetPref => {
    var element = document.getElementById('root');
    element.innerHTML = "";
    root = new Pan(element, undefined, 10, _updateViewsPref);
    (views = []).push(_makeInstance(root.first), _makeInstance(root.second));
    if (resetPref) _updateViewsPref();
  };

  window.balanceViews = savePref => {
    root.balance();
    if (savePref) _updateViewsPref();
  }

  buffer = _getPref('buffer', "");
  resetViews();

  var prefViews = _getPref('views')
    , prefTheme = _getPref('theme');
  if (prefViews) _loadViews(prefViews);
  if (prefTheme) _updateThemes(prefTheme);
})();
