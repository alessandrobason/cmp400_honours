framework:
----
# Core
- common
  - substitute for stdint/stddef
  - couple of useful macros
  - TCHAR char/wchar_t depending on UNICODE define
- handle
  - a checked pointer type
- ini
  - parses ini files
- input
  - mouse input (can be or'd: MOUSE_LEFT | MOUSE_RIGHT)
  - keyboard input
  - action input (virtual keys)
  - key to name
- options
  - hot reloaded options from ini conf file
- system
  - gfx/windowing init/cleanup
  - gfx data (device, context)
- timer
  - OnceClock (fires only once)
  - IntervalClock (fires every n seconds)
  - CPUClock (ns precision CPU clock)
  - GPUClock (us precision async GPU clock)
- tracelog
  - logging stuff (logMessage)
  - logger ui
- maths
  - some useful math stuff:
    - constants: pi, pi2 (pi * 2)
    - angle conversion: torad/todeg
    - min/max (std::min/std::max)
    - clamp: clamps a value between min and max
- vec
  - vector maths stuff for 2, 3, and 4 components
  - support multiple names for same values (x,y or u,v)
  - very similar to hlsl vectors
  - operator<=> returns a vec#T<bool> which can be checked with any or all functions
  - all support clamping
  - support norm() for normalizing
  - support cross() for cross product
  - support saturate() as clamp(v, 0, 1);
  - support dot() for dot product
  - support abs/round/ceil functions
  - any/all


# GFX
- buffer
  - can be const or structured
  - can be read/written by cpu/gpu
  - can be mapped (if CPU read/write)
  - has srv and uav
- camera
  - arcball camera that uses two angles (horizontal and vertical)
    to generate tje pos, fwd, up, and right vectors 
  - also manages zoom
  - manages input to check when it should sculpt
  - gets the mouse direction from screen-space to camera-space
- colour
  - colours????
- d3d11_fwd
  - forward stuff for d3d11, so we dont need to include the header (10k+ loc)
  - safeRelease function which internally calls ptr->Release if
    the ptr is not null, needed for dxptr
  - dxptr unique_ptr-like struct for d3d11 resources
  - some defines for win32:
    - win32_handle_t HANDLE (internally void ptr)
    - win32_overlapped_t so we don't need windows.h in utils.h
      could potentially be solved by just allocating that on the
      heap
- gfx_common
  - GFX_CLASS_CHECK checks that c++ struct is 128bit aligned, 
    needed for classes that are used as constant buffers
  - ShaderType used in a bunch of GFX classes to denote which
    shader is used
- gfx_factory
  - manages one type of GFX resource
  - uses a virtual allocator to keep pointers stable
  - internally has a linked list of all the the resources,
    this makes it very easy to pop out the last, also
    can support (not implemented as it was not really needed in
    this project) reuse of resources
  - subscribes itself to a list of factories (which is kept in 
    system.cc) that will clean it up when exiting the application
- mesh
  - simple mesh, only used once for full-screen triangle
- sculpture
  - manages sculpture stuff
  - has sculpt, scale shader
  - can save to file
    - when saved, it keeps track of the path/quality and autosaves
    - saving is done asyncronously
    - if sculpture isn't saved when closing, it prompts the user to save 
- shader
  - hot reloaded by default
  - load (from file)
  - compile (from file, supports macros)
  - hasUpdated (check if it has hot reloaded)
  - is either vs, fs, or cs
  - if vs has an input layout by default
  - if fs has a sampler by default
  - if cs can add sampler
  - binds buffers and srv with slices
    - this means that we can bind them all at once!
  - dispatch uses slices for everything, nice api!
- texture
  - 2D
    - load from file (normal/hdr)
    - async load (using promise)
    - take screenshot (save to file)
    - clear
    - copy into
  - 3D
    - create (w, h, d)
    - load (from file)
    - save (async)
  - Render Target
    - create (w, h)
    - fromBackbuffer
    - resize
    - reload backbuffer (if bb is resized)
    - bind
    - clear

# GUI
- brush_editor
  - manages all the brushes
  - findBrush: finds the first intersection of the mouse in
    a volume texture
  - runFillShader: fills a texture with a specific shape
    this is more correct than creating a brush and filling
    the texture as its using an sdf function.
- material_editor
  - manages material stuff
  - can load textures asyncrounously
  - update returns true if the ray tracer should render the 
    scene again
- ray_tracing_editor
  - manages ray tracing stuff
  - keeps track of how many frames have been rendered and how long each of them took
  - keeps track of how long it has been rendering
  - supports rendering a single frame as a preview
  - renders the scene a little bit every frame, this way it doesn't stall the whole thread every time
- widgets
  - collection of useful widgets:
    - fps: draws an fps widget on the bottom left corner
    - imageView: render an image, keeps aspect ratio intact
    - mainView: renders main rtv
    - messages: renders temporary messages that appear on the top left corner
    - keyRemapper: window to remap actions
    - controlsPage: window to display current controls
    - menuBar: menu bar at the top
    - saveLoadFile: not a widget technically, but needed as otherwise imgui freaks out whenever nfd is opened
  - imgui extended:
    - filledSlider: a slider that is filled until the value, instead of having an handle
    - input U8/Uint/Uint2/Uint3
    - slider UInt/UInt2/UInt3
    - separatorText: this: --- text -----------
    - tooltip: small (?) thing that has a message when hovered

# Utils
- mem
  - zero (memset 0)
  - copy (memcpy)
  - remref, remarr (type_traits)
  - move (std::move using remref)
  - swap (std::swap)
  - placement new (like new(), but a bit worse)
  - ptr (std::unique_ptr)
  - virtual allocator
    - can only allocate, deallocates all at once
    - does NOT call the destructor, it is very low level, just calloc pretty much
- arr (std::vector)
- slice (constant std::span with initializer_list support)
- str
  - tstr (TCHAR stuff)
  - view (std::string_view)
  - str::cmp, str::ncmp
  - ansiToWide/wideToAnsi
  - format, formatV, formatBuf, formatBufV, formatStr
  - str::dup
  - toInt, toUInt, toNum
- fs
  - MemoryBuf
  - Watcher
  - file (small wrapper around FILE * with destructor)
  - exists
  - fs::read
  - fs::write
  - findFirstAvailable
  - getFilename
  - getExtension
  - getNameAndExt
  - stream out
  - stream in
- thr
  - Mutex (std::mutex)
  - Promise (std::future, std::promise)

- substituting:
  - algorithm (min, max): 8689
  - memory (unique_ptr): 3492
  - vector: 2995
  - type_traits: 1995
  - future: 1182
  - ostream: 1013 
  - utility (move, swap): 789
  - mutex: 758
  - istream: 743 
  - span: 569
  - fstream: 455
  - string: 454
  - new: 97
  total std: 23295

  - arr: 123 //
  - fs: 103 352
  - mem: 86 63
  - thr: 66 47
  - slice: 45 //
  - str: 59 308
  headers: 482
  sources: 770
  total utils: 1252


# UX

- [X] vertical editors vs horizontal views and horizontal logger
  - [X] how verticality shaped design
- [X] material editor
  - [X] subdivided into sections
  - [X] tips for anything that could be confusing
  - [X] small colour preview, can be enlarged by clicking on it
  - [X] names are not necessarely correct, but easier to understand and not-technical
  - [X] preview of the textures which can be zoomed in when hovered
- [X] brush editor
  - [X] keyboard shortcuts
    - [X] Q-E to swap between brush and eraser (E is widely known as eraser and Q is close on the KB (considerations for non-qwerty KB?))
    - [X] Shift, Ctrl, and Alt bring up context menu right over the mouse
  - [X] default values
    - [X] default depth value is 0.9 as it feels the closest to real sculpting
    - [X] depth value swaps when changing between eraser and brush
  - [X] tips
    - [X] depth tip
- [X] ray tracing editor
  - [X] names are descriptive
  - [X] tips on every value
  - [X] save image button for ease of use
  - [X] rendering widget
    - [X] rendering time
    - [X] n of frames rendered
      - [X] only "objective" value
    - [X] how long each frame takes

- [x] main view
  - [x] image is always centred
  - [x] box to show editable area
  - [x] turns gray when disabled
- [x] render view
  - [-] automatically updates
- [-] logger
  - [-] can be filtered by type of message
  - [-] not useful for artists most of the time, but if something goes wrong it gives useful information for the programmers to understand what went wrong

- [-] other windows
  - [-] key remapping
  - [-] options
    - [-] helpful explanation for each option, takes for granted that the user has no tech experience
  - [-] controls
    - [-] list of useful controls
    - [-] not hard-coded, controls are updated when remapped
- [X] menu bar
  - [x] shortcuts (file->new/save)
  - [x] it also remember what you last put in
  - [x] quality level presets
  - [x] uses native file dialogue
  - [x] warns if you did not save after exiting
  - [x] autosave after first save!

- [x] widgets messages

- [X] layout
  - [X] windows can be moved around
  - [X] imgui automatically saves the last layout

# Lit Review
1. introduction to stuff
2. issues and challenges
  - many iterations close to the edges
  - 
3. main methods 
4. 

# testing

- questionnaire
  - intuitiveness
  - ease of use
  - background
  - scale from 1-5
  - extra space for tips/critiques
  
- performance
  - fps average for the same complex sculpture at different sizes
  - ms taken for brush sculpting operation at different brush sizes
  - ms taken for eraser sculpting operation at different brush sizes