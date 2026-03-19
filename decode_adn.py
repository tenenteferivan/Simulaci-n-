import sys

def decode_chunk(chunk):
    try:
        # solo números
        num = int(''.join(filter(str.isdigit, chunk)))
        return num
    except:
        return None

def pretty_print(adn_raw):
    chunk_size = 50  # ajusta según tu memoria
    for i in range(0, len(adn_raw), chunk_size):
        chunk = adn_raw[i:i+chunk_size]
        val = decode_chunk(chunk)
        if val is not None:
            print("Chunk decodificado:", val)
        else:
            print("Chunk inválido, ignorado")

if __name__ == "__main__":
    adn_raw = sys.stdin.read().strip().replace("\n","").replace(" ","")
    pretty_print(adn_raw)