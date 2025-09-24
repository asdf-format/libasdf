import re
from datetime import datetime
from pathlib import Path


# -- Project information ------------------------------------------------------
def read_config_h() -> tuple[str, str, str]:
    """Read package data out of config.h if possible"""
    project = 'libasdf'
    version = '0.0'
    release = '0.0.0'

    config_h_path = Path(__file__).parent.parent / 'config.h'

    if not config_h_path.is_file():
        return project, version, release

    content = config_h_path.read_text()
    if (m := re.search(r'#define\s+PACKAGE_NAME\s+"([^"]+)"', content)):
        project = m.group(1)

    if (m := re.search(r'#define\s+PACKAGE_VERSION\s+"([^"]+)"', content)):
        release = m.group(1)

    version = '.'.join(release.split('.')[:2])
    return project, version, release


project, version, release = read_config_h()
author = 'The ASDF Developers'
copyright = f"{datetime.now().year}, {author}"

# It is a C library--use the 'c' domain by default
primary_domain = 'c'
default_role = 'c:expr'

# -- Options for HTML output ---------------------------------------------------
html_title = f"{project} v{release}"

# Output file base name for HTML help builder.
htmlhelp_basename = project + "doc"

# -- Options for LaTeX output --------------------------------------------------
latex_documents = [(
    "index",
    project + ".tex",
    project + " Documentation", author, "manual"
)]

# -- Options for manual page output --------------------------------------------
man_pages = [("index", project.lower(), project + " Documentation", [author], 1)]


todo_include_todos = True


# Epilogue appended to each rst file; use this to append commonly used link
# references
rst_epilog = ''

with open('links.rst') as fobj:
    rst_epilog += fobj.read()


# Enable nitpicky mode - which ensures that all references in the docs
# resolve.

nitpicky = True

# Nitpicks to ignore
# Because we use c:expr as the default role which is *very* convenient, any
# standard C identifiers used within backticks will try to resolve as well.
# I haven't found any Sphinx documents that cover the C standard library
# (someone should write one!) so we list most of those here when they come up
# in the docs.  Try to keep this sorted...
nitpick_ignore = [
    ('c:identifier', 'ERANGE'),
    ('c:identifier', 'FILE'),
    ('c:identifier', 'NULL'),
    ('c:identifier', 'errno'),
    ('c:identifier', 'int16_t'),
    ('c:identifier', 'int32_t'),
    ('c:identifier', 'int64_t'),
    ('c:identifier', 'int8_t'),
    ('c:identifier', 'size_t'),
    ('c:identifier', 'strtod'),
    ('c:identifier', 'uint16_t'),
    ('c:identifier', 'uint32_t'),
    ('c:identifier', 'uint64_t'),
    ('c:identifier', 'uint8_t'),
]

# Add intersphinx mappings
# e.g. intersphinx_mapping["semantic_version"] = ("https://python-semanticversion.readthedocs.io/en/latest/", None)
intersphinx_mapping = {
    'asdf-standard': ('https://www.asdf-format.org/projects/asdf-standard/en/latest/', None)
}

extensions = ['sphinx.ext.intersphinx', 'sphinx.ext.todo', 'hawkmoth']

# -- Options for hawkmoth extension --------------------------------------------

hawkmoth_root = Path(__file__).parent.parent

# These are options that should be passed to the compiler when hawkmoth processes
# files.
#
# Should see if we can glean what we need here from configure/automake output
# For now see what we can get away with by simply hard-coding...
hawkmoth_clang = [f'-I{hawkmoth_root}/include', '-Iinclude']


# -- Options for theme and HTML output -----------------------------------------
html_theme = "furo"
html_static_path = ["_static"]
# Override default settings from sphinx_asdf / sphinx_astropy (incompatible with furo)
html_sidebars = {}
# The name of an image file (within the static path) to use as favicon of the
# docs.  This file should be a Windows icon file (.ico) being 16x16 or 32x32
# pixels large.
html_favicon = "_static/images/favicon.ico"
html_logo = ""

globalnavlinks = {
    "ASDF Projects": "https://www.asdf-format.org",
    "Tutorials": "https://www.asdf-format.org/en/latest/tutorials/index.html",
    "Community": "https://www.asdf-format.org/en/latest/community/index.html",
}

topbanner = ""
for text, link in globalnavlinks.items():
    topbanner += f"<a href={link}>{text}</a>"

html_theme_options = {
    "light_logo": "images/logo-light-mode.png",
    "dark_logo": "images/logo-dark-mode.png",
    "announcement": topbanner,
}

pygments_style = "monokai"
# NB Dark style pygments is furo-specific at this time
pygments_dark_style = "monokai"

# -- Options for LaTeX output --------------------------------------------------

# Grouping the document tree into LaTeX files. List of tuples
# (source start file, target name, title, author, documentclass [howto/manual]).
latex_documents = [("index", project + ".tex", project + " Documentation", author, "manual")]

latex_logo = "_static/images/logo-light-mode.png"


def setup(app):
    app.add_css_file("css/globalnav.css")
