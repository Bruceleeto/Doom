#!/usr/bin/env python3
"""
proc2bin.py — Convert Doom 3 ASCII .proc files to binary format.

Strips shadow models, packs geometry into compact binary.
Output replaces the .proc file with a binary version.

Binary format (DCPROC01):
  Header:
    8 bytes  magic "DCPROC01"
    uint32   numModels
    uint32   numPortalAreas
    uint32   numPortals
    uint32   numNodes

  Per model:
    uint16   nameLen (including null)
    char[]   name (null-terminated)
    uint32   numSurfaces
    Per surface:
      uint16   materialLen (including null)
      char[]   material (null-terminated)
      uint32   numVerts
      uint32   numIndexes
      float32[numVerts * 8]   vertex data (xyz, st, normal)
      uint32[numIndexes]      index data

  Portals section:
    Per portal:
      uint32   numPoints
      uint32   area1
      uint32   area2
      float32[numPoints * 3]  point data (xyz)

  Nodes section:
    Per node:
      float32[4]   plane (nx, ny, nz, d)
      int32        child0
      int32        child1
"""

import struct
import re
import sys
import os
import glob


def tokenize(text):
    """Simple tokenizer for Doom 3 ASCII formats."""
    i = 0
    n = len(text)
    while i < n:
        # skip whitespace
        while i < n and text[i] in ' \t\r\n':
            i += 1
        if i >= n:
            break
        # skip comments
        if text[i] == '/' and i + 1 < n and text[i+1] == '/':
            while i < n and text[i] != '\n':
                i += 1
            continue
        if text[i] == '/' and i + 1 < n and text[i+1] == '*':
            i += 2
            while i < n - 1 and not (text[i] == '*' and text[i+1] == '/'):
                i += 1
            i += 2
            continue
        # quoted string
        if text[i] == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 1
            yield text[i+1:j]
            i = j + 1
            continue
        # parentheses as tokens
        if text[i] in '(){}':
            yield text[i]
            i += 1
            continue
        # number or identifier
        j = i
        while j < n and text[j] not in ' \t\r\n(){}/"':
            j += 1
        yield text[i:j]
        i = j


def parse_proc(filepath):
    """Parse an ASCII .proc file and return structured data."""
    with open(filepath, 'r') as f:
        text = f.read()

    tokens = list(tokenize(text))
    pos = [0]  # mutable index

    def next_tok():
        t = tokens[pos[0]]
        pos[0] += 1
        return t

    def expect(s):
        t = next_tok()
        assert t == s, f"Expected '{s}', got '{t}' at token {pos[0]-1}"

    def parse_int():
        return int(next_tok())

    def parse_float():
        return float(next_tok())

    def parse_matrix(count):
        expect('(')
        vals = [parse_float() for _ in range(count)]
        expect(')')
        return vals

    # header
    magic = next_tok()
    assert magic == 'mapProcFile003', f"Bad magic: {magic}"

    models = []
    portals = None
    nodes = None

    while pos[0] < len(tokens):
        tok = next_tok()

        if tok == 'model':
            expect('{')
            name = next_tok()
            num_surfaces = parse_int()
            surfaces = []
            for _ in range(num_surfaces):
                expect('{')
                material = next_tok()
                num_verts = parse_int()
                num_indexes = parse_int()
                verts = []
                for _ in range(num_verts):
                    verts.extend(parse_matrix(8))
                indexes = [parse_int() for _ in range(num_indexes)]
                expect('}')
                surfaces.append({
                    'material': material,
                    'num_verts': num_verts,
                    'num_indexes': num_indexes,
                    'verts': verts,      # flat list of floats, 8 per vert
                    'indexes': indexes,
                })
            expect('}')
            models.append({'name': name, 'surfaces': surfaces})

        elif tok == 'shadowModel':
            # skip the entire braced section
            expect('{')
            depth = 1
            while depth > 0:
                t = next_tok()
                if t == '{':
                    depth += 1
                elif t == '}':
                    depth -= 1

        elif tok == 'interAreaPortals':
            expect('{')
            num_areas = parse_int()
            num_portals = parse_int()
            portal_list = []
            for _ in range(num_portals):
                num_points = parse_int()
                a1 = parse_int()
                a2 = parse_int()
                points = []
                for _ in range(num_points):
                    points.extend(parse_matrix(3))
                portal_list.append({
                    'num_points': num_points,
                    'area1': a1,
                    'area2': a2,
                    'points': points,
                })
            expect('}')
            portals = {'num_areas': num_areas, 'portals': portal_list}

        elif tok == 'nodes':
            expect('{')
            num_nodes = parse_int()
            node_list = []
            for _ in range(num_nodes):
                plane = parse_matrix(4)
                c0 = parse_int()
                c1 = parse_int()
                node_list.append({'plane': plane, 'children': [c0, c1]})
            expect('}')
            nodes = node_list

        else:
            print(f"Warning: unknown token '{tok}', skipping")

    return models, portals, nodes


def write_binary(filepath, models, portals, nodes):
    """Write binary .proc file."""
    num_areas = portals['num_areas'] if portals else 0
    num_portals = len(portals['portals']) if portals else 0
    num_nodes = len(nodes) if nodes else 0

    with open(filepath, 'wb') as f:
        # header
        f.write(b'DCPROC01')
        f.write(struct.pack('<IIII', len(models), num_areas, num_portals, num_nodes))

        # models
        for model in models:
            name_bytes = model['name'].encode('utf-8') + b'\x00'
            f.write(struct.pack('<H', len(name_bytes)))
            f.write(name_bytes)
            f.write(struct.pack('<I', len(model['surfaces'])))

            for surf in model['surfaces']:
                mat_bytes = surf['material'].encode('utf-8') + b'\x00'
                f.write(struct.pack('<H', len(mat_bytes)))
                f.write(mat_bytes)
                f.write(struct.pack('<II', surf['num_verts'], surf['num_indexes']))
                # vertex data: 8 floats per vert
                f.write(struct.pack(f"<{len(surf['verts'])}f", *surf['verts']))
                # index data: uint32
                f.write(struct.pack(f"<{len(surf['indexes'])}I", *surf['indexes']))

        # portals
        if portals:
            for portal in portals['portals']:
                f.write(struct.pack('<III', portal['num_points'], portal['area1'], portal['area2']))
                f.write(struct.pack(f"<{len(portal['points'])}f", *portal['points']))

        # nodes
        if nodes:
            for node in nodes:
                f.write(struct.pack('<4f', *node['plane']))
                f.write(struct.pack('<ii', *node['children']))


def convert_proc(inpath, outpath=None):
    """Convert a single .proc file from ASCII to binary."""
    if outpath is None:
        outpath = inpath

    print(f"  Parsing {inpath}...")
    models, portals, nodes = parse_proc(inpath)

    total_verts = sum(s['num_verts'] for m in models for s in m['surfaces'])
    total_idx = sum(s['num_indexes'] for m in models for s in m['surfaces'])
    print(f"    {len(models)} models, {total_verts} verts, {total_idx} indexes")
    if portals:
        print(f"    {portals['num_areas']} areas, {len(portals['portals'])} portals")
    if nodes:
        print(f"    {len(nodes)} BSP nodes")

    write_binary(outpath, models, portals, nodes)
    in_size = os.path.getsize(inpath) if inpath != outpath else 0
    out_size = os.path.getsize(outpath)
    print(f"    Binary: {out_size:,} bytes")


def main():
    if len(sys.argv) < 2:
        print("Usage: proc2bin.py <file.proc|directory>")
        sys.exit(1)

    target = sys.argv[1]

    if os.path.isfile(target):
        convert_proc(target)
    elif os.path.isdir(target):
        procs = glob.glob(os.path.join(target, '**/*.proc'), recursive=True)
        if not procs:
            print(f"No .proc files found in {target}")
            sys.exit(1)
        print(f"Converting {len(procs)} .proc files...")
        for p in sorted(procs):
            convert_proc(p)
        print("Done.")
    else:
        print(f"Not found: {target}")
        sys.exit(1)


if __name__ == '__main__':
    main()
