"""
Group index entries by the first letter that actually distinguishes them.

In a C library where there is only a global namespace, and nearly every symbol
shares a prefix (i.e. ``asdf_``, ``ASDF_``, and so on) the generated index
files all of them under a single letter and is close to useless for finding
anything.

Sphinx builds each index heading from the *category key* of an index entry: the
fifth element of the ``(type, value, target_id, main, key)`` tuples the domains
emit.  It falls back to the entry's first letter only when that key is ``None``
(see ``sphinx.environment.adapters.indexentries._group_by_func``), and when the
key is set it sorts by it too.  That hook exists for CJK indexes, where a
first-letter heading is meaningless, but it serves just as well here.

Setting it lets each symbol be filed under the first letter after its common
prefix, so ``asdf_value_as_string`` lands under "V".  The entry text itself is
left alone, so searching still matches the full name.

Configuration
-------------

``index_strip_prefixes``
    Prefixes to look past when choosing a heading, e.g.
    ``['asdf_gwcs_', 'ASDF_GWCS_', 'asdf_', 'ASDF_']``.  Longest match wins.
    Empty (the default) disables the extension.
"""


def _group_key(entry_text, prefixes):
    """The letter to file an entry under, or None for Sphinx's default."""
    # Index text looks like "asdf_value_as_string (C function)", and for a
    # struct member "asdf_time_t.value (C member)".  Group members with their
    # parent so a struct and its fields never land under different letters.
    name = entry_text.split(' ', 1)[0].split('.', 1)[0].strip()

    # Longest first, so 'asdf_gwcs_' wins over 'asdf_'.  A prefix that would
    # leave a single character behind is skipped in favour of a shorter one:
    # 'asdf_gwcs_t' is far easier to find under G, alongside its own members,
    # than alone under T.
    for prefix in sorted(prefixes, key=len, reverse=True):
        if not name.startswith(prefix):
            continue

        rest = name[len(prefix):].lstrip('_')

        if len(rest) > 1 and rest[:1].isalpha():
            return rest[0].upper()

    return None


def _regroup_index_entries(app, env):
    prefixes = app.config.index_strip_prefixes

    if not prefixes:
        return

    domain = env.domains['index']

    for docname, entries in domain.entries.items():
        regrouped = []

        for entry_type, value, target_id, main, category_key in entries:
            if entry_type == 'single' and category_key is None:
                category_key = _group_key(value, prefixes)

            regrouped.append((entry_type, value, target_id, main, category_key))

        domain.entries[docname] = regrouped


def setup(app):
    app.add_config_value('index_strip_prefixes', [], 'env')
    app.connect('env-check-consistency', _regroup_index_entries)

    return {
        'version': '0.1',
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
