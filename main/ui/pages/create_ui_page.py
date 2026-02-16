#!/usr/bin/env python3
"""
Create a new UI page by copying `ui_page_template.c` and updating
`pages.def` and `main/CMakeLists.txt`.

Usage: create_ui_page.py Name

This will create `ui_page_<name>.c` (name lowercased), add `X_PAGE(NAME)`
to `pages.def` (uppercased), and add the new source to `main/CMakeLists.txt`.
"""
import argparse
import os
import re
import sys


def make_variants(name: str):
    lower = name.lower()
    upper = name.upper()
    cap = name[0].upper() + name[1:] if name else name
    return lower, cap, upper


def replace_case_variants(text: str, lower: str, cap: str, upper: str) -> str:
    # Replace in order: upper, capitalized, lower to avoid accidental double-replace
    text = text.replace('TEMPLATE', upper)
    text = text.replace('Template', cap)
    text = text.replace('template', lower)
    return text


def ensure_pages_def(pages_def_path: str, upper: str):
    line = f'X_PAGE({upper})'
    with open(pages_def_path, 'r', encoding='utf-8') as f:
        contents = f.read()
    if line in contents:
        print(f"pages.def already contains {line}")
        return False
    # Append with newline if file doesn't end with one
    if not contents.endswith('\n'):
        contents += '\n'
    contents += line + '\n'
    with open(pages_def_path, 'w', encoding='utf-8') as f:
        f.write(contents)
    print(f"Appended {line} to {pages_def_path}")
    return True


def ensure_cmake_entry(cmake_path: str, lower: str):
    entry = f'"ui/pages/ui_page_{lower}.c"'
    with open(cmake_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    # If entry already present, nothing to do
    if any(entry in l for l in lines):
        print(f"CMakeLists already contains {entry}")
        return False

    # Find the existing ui_page_template.c line and insert after it.
    target = '"ui/pages/ui_page_template.c"'
    for idx, l in enumerate(lines):
        if target in l:
            indent = re.match(r"^(\s*)", l).group(1)
            insert_line = indent + entry + '\n'
            lines.insert(idx + 1, insert_line)
            with open(cmake_path, 'w', encoding='utf-8') as f:
                f.writelines(lines)
            print(f"Inserted {entry} into {cmake_path} after {target}")
            return True

    # Fallback: append near the UI elements comment if target not found
    for idx, l in enumerate(lines):
        if 'UI elements' in l:
            # insert a few lines after
            insert_line = '    ' + entry + '\n'
            lines.insert(idx + 2, insert_line)
            with open(cmake_path, 'w', encoding='utf-8') as f:
                f.writelines(lines)
            print(f"Appended {entry} into {cmake_path} near UI elements")
            return True

    # Last resort: append to end
    with open(cmake_path, 'a', encoding='utf-8') as f:
        f.write('\n' + entry + '\n')
    print(f"Appended {entry} to end of {cmake_path}")
    return True


def main():
    parser = argparse.ArgumentParser(description='Create a new UI page from template')
    parser.add_argument('name', help='New page name token (e.g. example or Name)')
    parser.add_argument('--dry-run', action='store_true', help='Show actions without writing files')
    args = parser.parse_args()

    name = args.name.strip()
    if not re.match(r'^[A-Za-z0-9_]+$', name):
        print('Name must be alphanumeric or underscore only')
        return 2

    lower, cap, upper = make_variants(name)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    template_path = os.path.join(script_dir, 'ui_page_template.c')
    if not os.path.exists(template_path):
        print(f'Template file not found: {template_path}')
        return 3

    with open(template_path, 'r', encoding='utf-8') as f:
        content = f.read()

    new_content = replace_case_variants(content, lower, cap, upper)

    new_filename = f'ui_page_{lower}.c'
    new_path = os.path.join(script_dir, new_filename)

    if os.path.exists(new_path):
        print(f'Target file already exists: {new_path}')
        return 4

    if args.dry_run:
        print('Dry-run: would create', new_path)
    else:
        with open(new_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f'Created {new_path}')

    # Update pages.def
    pages_def_path = os.path.join(script_dir, 'pages.def')
    if not os.path.exists(pages_def_path):
        print(f'pages.def not found: {pages_def_path}')
        return 5
    if not args.dry_run:
        ensure_pages_def(pages_def_path, upper)
    else:
        print('Dry-run: would append X_PAGE(%s) to %s' % (upper, pages_def_path))

    # Update main/CMakeLists.txt
    cmake_path = os.path.abspath(os.path.join(script_dir, '..', '..', 'CMakeLists.txt'))
    if not os.path.exists(cmake_path):
        print(f'CMakeLists.txt not found: {cmake_path}')
        return 6
    if not args.dry_run:
        ensure_cmake_entry(cmake_path, lower)
    else:
        print('Dry-run: would add ui/pages/%s to %s' % (new_filename, cmake_path))

    print('Done')
    return 0


if __name__ == '__main__':
    sys.exit(main())
