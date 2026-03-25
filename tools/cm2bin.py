#!/usr/bin/env python3
"""
cm2bin.py — Convert Doom 3 ASCII .cm files to binary format.

Binary format (DCCM0001):
  Header:
    8 bytes  magic "DCCM0001"
    uint32   mapFileCRC
    uint32   numModels

  Per model:
    uint16   nameLen (including null)
    char[]   name (null-terminated)
    uint32   numVertices
    uint32   numEdges
    uint32   numPolygons
    uint32   numBrushes

    Vertices:
      float32[numVertices * 3]   xyz positions

    Edges:
      Per edge:
        int32    vertexNum0
        int32    vertexNum1
        uint16   internal
        uint16   numUsers

    Nodes (flattened pre-order BSP):
      uint32   numNodes
      Per node:
        int32    planeType   (-1 = leaf)
        float32  planeDist

    Polygons:
      Per polygon:
        uint32   numEdges
        int32[numEdges]  edge indices (can be negative)
        float32[3]       normal
        float32          distance
        float32[3]       minBounds
        float32[3]       maxBounds
        uint16           materialLen (including null)
        char[]           material (null-terminated)

    Brushes:
      Per brush:
        uint32   numPlanes
        Per plane:
          float32[3]     normal
          float32        distance
        float32[3]       minBounds
        float32[3]       maxBounds
        uint16           contentsLen (including null)
        char[]           contents (null-terminated)
"""

import struct
import sys
import os
import glob


def tokenize(text):
    """Simple tokenizer for Doom 3 ASCII formats."""
    i = 0
    n = len(text)
    while i < n:
        while i < n and text[i] in ' \t\r\n':
            i += 1
        if i >= n:
            break
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
        if text[i] == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 1
            yield text[i+1:j]
            i = j + 1
            continue
        if text[i] in '(){}':
            yield text[i]
            i += 1
            continue
        j = i
        while j < n and text[j] not in ' \t\r\n(){}/"':
            j += 1
        yield text[i:j]
        i = j


def parse_cm(filepath):
    """Parse an ASCII .cm file."""
    with open(filepath, 'r') as f:
        text = f.read()

    tokens = list(tokenize(text))
    pos = [0]

    def next_tok():
        t = tokens[pos[0]]
        pos[0] += 1
        return t

    def peek_tok():
        return tokens[pos[0]] if pos[0] < len(tokens) else None

    def expect(s):
        t = next_tok()
        assert t == s, f"Expected '{s}', got '{t}' at token {pos[0]-1}"

    def parse_int():
        return int(next_tok())

    def parse_float():
        return float(next_tok())

    def parse_vec(count):
        expect('(')
        vals = [parse_float() for _ in range(count)]
        expect(')')
        return vals

    # header
    magic = next_tok()
    assert magic == 'CM', f"Bad magic: {magic}"
    version = next_tok()
    assert version == '1.00', f"Bad version: {version}"
    crc = parse_int()

    models = []

    while pos[0] < len(tokens):
        tok = next_tok()
        if tok == 'collisionModel':
            name = next_tok()
            expect('{')

            vertices = []
            edges = []
            nodes = []
            polygons = []
            brushes = []

            while peek_tok() != '}':
                section = next_tok()

                if section == 'vertices':
                    expect('{')
                    count = parse_int()
                    for _ in range(count):
                        v = parse_vec(3)
                        vertices.append(v)
                    expect('}')

                elif section == 'edges':
                    expect('{')
                    count = parse_int()
                    for _ in range(count):
                        verts = parse_vec(2)
                        internal = parse_int()
                        num_users = parse_int()
                        edges.append({
                            'v0': int(verts[0]),
                            'v1': int(verts[1]),
                            'internal': internal,
                            'numUsers': num_users,
                        })
                    expect('}')

                elif section == 'nodes':
                    expect('{')

                    def parse_node():
                        pv = parse_vec(2)
                        plane_type = int(pv[0])
                        plane_dist = pv[1]
                        nodes.append({
                            'planeType': plane_type,
                            'planeDist': plane_dist,
                        })
                        if plane_type != -1:
                            parse_node()
                            parse_node()

                    parse_node()
                    expect('}')

                elif section == 'polygons':
                    # skip the optional polygonMemory number
                    if peek_tok() != '{':
                        parse_int()
                    expect('{')
                    while peek_tok() != '}':
                        num_edges = parse_int()
                        expect('(')
                        edge_list = [parse_int() for _ in range(num_edges)]
                        expect(')')
                        normal = parse_vec(3)
                        dist = parse_float()
                        min_bounds = parse_vec(3)
                        max_bounds = parse_vec(3)
                        material = next_tok()
                        polygons.append({
                            'numEdges': num_edges,
                            'edges': edge_list,
                            'normal': normal,
                            'dist': dist,
                            'minBounds': min_bounds,
                            'maxBounds': max_bounds,
                            'material': material,
                        })
                    expect('}')

                elif section == 'brushes':
                    # skip the optional brushMemory number
                    if peek_tok() != '{':
                        parse_int()
                    expect('{')
                    while peek_tok() != '}':
                        num_planes = parse_int()
                        expect('{')
                        planes = []
                        for _ in range(num_planes):
                            n = parse_vec(3)
                            d = parse_float()
                            planes.append(n + [d])
                        expect('}')
                        min_bounds = parse_vec(3)
                        max_bounds = parse_vec(3)
                        contents = next_tok()
                        brushes.append({
                            'numPlanes': num_planes,
                            'planes': planes,
                            'minBounds': min_bounds,
                            'maxBounds': max_bounds,
                            'contents': contents,
                        })
                    expect('}')

            expect('}')

            models.append({
                'name': name,
                'vertices': vertices,
                'edges': edges,
                'nodes': nodes,
                'polygons': polygons,
                'brushes': brushes,
            })

    return crc, models


def write_binary_cm(filepath, crc, models):
    """Write binary .cm file."""
    with open(filepath, 'wb') as f:
        f.write(b'DCCM0001')
        f.write(struct.pack('<II', crc, len(models)))

        for model in models:
            name_bytes = model['name'].encode('utf-8') + b'\x00'
            f.write(struct.pack('<H', len(name_bytes)))
            f.write(name_bytes)

            nv = len(model['vertices'])
            ne = len(model['edges'])
            np = len(model['polygons'])
            nb = len(model['brushes'])
            f.write(struct.pack('<IIII', nv, ne, np, nb))

            # vertices
            for v in model['vertices']:
                f.write(struct.pack('<3f', *v))

            # edges
            for e in model['edges']:
                f.write(struct.pack('<iiHH', e['v0'], e['v1'], e['internal'], e['numUsers']))

            # nodes
            nn = len(model['nodes'])
            f.write(struct.pack('<I', nn))
            for node in model['nodes']:
                f.write(struct.pack('<if', node['planeType'], node['planeDist']))

            # polygons
            for poly in model['polygons']:
                f.write(struct.pack('<I', poly['numEdges']))
                f.write(struct.pack(f"<{poly['numEdges']}i", *poly['edges']))
                f.write(struct.pack('<3f', *poly['normal']))
                f.write(struct.pack('<f', poly['dist']))
                f.write(struct.pack('<3f', *poly['minBounds']))
                f.write(struct.pack('<3f', *poly['maxBounds']))
                mat_bytes = poly['material'].encode('utf-8') + b'\x00'
                f.write(struct.pack('<H', len(mat_bytes)))
                f.write(mat_bytes)

            # brushes
            for brush in model['brushes']:
                f.write(struct.pack('<I', brush['numPlanes']))
                for plane in brush['planes']:
                    f.write(struct.pack('<4f', *plane))
                f.write(struct.pack('<3f', *brush['minBounds']))
                f.write(struct.pack('<3f', *brush['maxBounds']))
                cont_bytes = brush['contents'].encode('utf-8') + b'\x00'
                f.write(struct.pack('<H', len(cont_bytes)))
                f.write(cont_bytes)


def convert_cm(inpath, outpath=None):
    """Convert a single .cm file from ASCII to binary."""
    if outpath is None:
        outpath = inpath

    print(f"  Parsing {inpath}...")
    crc, models = parse_cm(inpath)

    total_verts = sum(len(m['vertices']) for m in models)
    total_edges = sum(len(m['edges']) for m in models)
    total_polys = sum(len(m['polygons']) for m in models)
    total_brushes = sum(len(m['brushes']) for m in models)
    print(f"    {len(models)} models, {total_verts} verts, {total_edges} edges, {total_polys} polys, {total_brushes} brushes")

    write_binary_cm(outpath, crc, models)
    out_size = os.path.getsize(outpath)
    print(f"    Binary: {out_size:,} bytes")


def main():
    if len(sys.argv) < 2:
        print("Usage: cm2bin.py <file.cm|directory>")
        sys.exit(1)

    target = sys.argv[1]

    if os.path.isfile(target):
        convert_cm(target)
    elif os.path.isdir(target):
        cms = glob.glob(os.path.join(target, '**/*.cm'), recursive=True)
        if not cms:
            print(f"No .cm files found in {target}")
            sys.exit(1)
        print(f"Converting {len(cms)} .cm files...")
        for p in sorted(cms):
            convert_cm(p)
        print("Done.")
    else:
        print(f"Not found: {target}")
        sys.exit(1)


if __name__ == '__main__':
    main()
