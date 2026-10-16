.pragma library

// PULSAR design tokens. Dark, neutral and cinematic: accent blue is reserved
// for selection, primary actions, playhead, focus and progress.
// Names in the "Semantic" block are the contract; the legacy aliases below
// keep older components working and should not be used in new code.

// ---- Surfaces (darkest -> lightest) --------------------------------------
var bgRoot     = "#0a0c10"   // window / gutters
var bgPanel    = "#11141a"   // panels, monitor module
var bgElevated = "#171b22"   // controls, track headers, rows
var bgHover    = "#1e232c"
var bgSelected = "#18283f"   // selected row / tinted selection
var bgViewer   = "#030405"   // video mat, near-pure black
var bgInput    = "#0d1015"
var bgPressed  = "#222833"

// Public semantic surface names. Components should describe intent rather
// than a particular layer number; the compact aliases keep that vocabulary
// stable if the graphite palette is tuned later.
var backgroundBase   = bgRoot
var backgroundPanel  = bgPanel
var backgroundRaised = bgElevated
var backgroundViewer = bgViewer

// ---- Text -----------------------------------------------------------------
var textPrimary   = "#e8ebf0"
var textSecondary = "#9ba4b1"
var textMuted     = "#737d8a"
var textOnAccent  = "#ffffff"
var textOnWarning = "#1a1306"
var chipOverlay   = "#66000000"   // translucent chip over clips

// ---- Lines ----------------------------------------------------------------
var borderSubtle = "#1f242d"
var borderStrong = "#2e3541"   // inputs, focus-less outlines

// ---- Accent ---------------------------------------------------------------
var accent        = "#4d8dff"
var accentHover   = "#6ba0ff"
var accentPressed = "#3a74e0"
var accentDim     = "#1c3358"   // tinted accent background
var focus         = "#8db8ff"

// ---- Status ---------------------------------------------------------------
var success = "#3fcf8e"
var warning = "#e3ab4f"
var error   = "#f26d6d"

// Translucent state fills. Keeping alpha here avoids slightly different
// greens/reds and modal scrims appearing throughout the interface.
var overlayScrim       = "#99000000"
var overlaySoft        = "#33000000"
var overlayStrong      = "#66000000"
var dropValidFill      = "#443fcf8e"
var dropInvalidFill    = "#44f26d6d"
var mutedActive        = "#3a2f18"
var mutedActiveHover   = "#4a3a1c"
var lightBorder        = "#33ffffff"
var strongLightLine    = "#ccffffff"

// ---- Timeline content -----------------------------------------------------
var clipVideo         = "#243449"
var clipVideoHover    = "#2b3e56"
var clipVideoSelected = "#2b4a78"
var clipAudio         = "#1d3a3a"
var clipAudioHover    = "#244746"
var clipAudioSelected = "#25524f"
var clipMissing       = "#34202a"
var clipImage         = "#3a3225"   // photos and colour cards
var clipImageHover    = "#463b2b"
var clipImageSelected = "#5b4930"
var imageInk          = "#e8b86b"
var title             = "#40305f"       // title track
var titleHover        = "#4a3870"
var titleSelected     = "#5a428a"
var laneTitle         = "#0f0d15"
var laneVideo = "#0e1116"
var laneAudio = "#0c0f13"
var ruler     = "#0d1015"
var track     = "#252c37"       // slider rails

var waveform            = "#8dbbff"   // video clip (embedded audio)
var waveformSelected    = "#d9e8ff"
var waveformAudio       = "#6fe0bd"   // audio-only clip
var waveformAudioSelected = "#d6fbef"
var waveformWell        = "#59000000" // dark inset behind waveforms
var waveformPending     = "#d3a85d"
var waveformBaseline    = "#56667b"
var waveformUnavailable = "#69717c"

// ---- Geometry -------------------------------------------------------------
var radiusS = 4
var radiusM = 6
var radiusL = 8

var spaceXS = 4
var spaceS  = 8
var spaceM  = 12
var spaceL  = 16
var spaceXL = 24

var controlHeight = 30
var controlHeightSmall = 26
var motionFast = 90

// ---- Type -----------------------------------------------------------------
var fontSizeXS = 11   // captions, chips
var fontSizeS  = 12   // metadata, secondary
var fontSizeM  = 13   // body, controls
var fontSizeL  = 14   // panel titles
var fontSizeXL = 15   // wordmark
var fontSizeTC = 22   // timecode
var fontSizeXXL = 26  // empty-state mark
var mono = "monospace"

// ---- Legacy aliases (do not use in new code) ------------------------------
var base = bgRoot
var workspace = bgRoot
var header = bgRoot
var panel = bgPanel
var panelAlt = bgElevated
var surfaceRaised = bgElevated
var monitor = bgPanel
var viewer = bgViewer
var lane = laneVideo
var input = bgInput
var border = borderStrong
var borderSoft = borderSubtle
var edge = "#3a424f"
var text = textPrimary
var textBright = "#f3f5f8"
var muted = textSecondary
var faint = textMuted
var buttonBase = bgElevated
var buttonHover = bgHover
var buttonPressed = bgPressed
var buttonDisabled = "#12151a"
var clipBase = clipVideo
var clipHover = clipVideoHover
var clipSelected = clipVideoSelected
var missingClip = clipMissing
var danger = error
var disabledText = "#566170"
var panelPadding = 12
var badge = "#aa101820"
