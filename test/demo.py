import logging
import sys


logging.basicConfig(level=logging.INFO)


def usage():
    print("usage: demo.py [FILE_NAME] [TABLE_SIZE]")


def main():
    if len(sys.argv) != 2:
        usage()
        print("Numero di parametri errato")
        exit(1)

    file_name = sys.argv[1]

    if file_name == '':
        usage()
        print("Nome del file errato")
        exit(3)

    hashmap = {}

    print("Inizio fase INSERIMENTO..")

    with open(file_name, 'r') as f:
        for line in f:
            key = line.strip('\n')
            logging.debug(f"Inserting key {key}")
            if key not in hashmap:
                logging.debug("Key NOT found!")
                hashmap[key] = 1
            else:
                logging.debug(f"Key already FOUND! Incrementing: {hashmap[key]}")
                hashmap[key] += 1

    print("Dimensione della HashTable: ", len(hashmap))
    print("Inizio fase RIMOZIONE..")

    with open(file_name, 'r') as f:
        for line in f:
            key = line.strip('\n')
            if key in hashmap:
                del hashmap[key]

    print("Dimensione della HashTable: ", len(hashmap))


if __name__ == "__main__":
    main()