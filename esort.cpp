#include <cstdio>
#include <cstdlib>

// ========================== DataSource (Abstracta) ==========================

class DataSource {
public:
    virtual int getNext() = 0;
    virtual bool hasMoreData() = 0;
    virtual ~DataSource() {}
};

// ========================== SerialSource (Arduino) ==========================

class SerialSource : public DataSource {
private:
    FILE* file;
    bool eof;
    int currentValue;

    void readNextInternal() {
        if (file == NULL) {
            eof = true;
            return;
        }
        int temp;
        int result = std::fscanf(file, "%d", &temp);
        if (result == 1) {
            currentValue = temp;
            eof = false;
        } else {
            eof = true;
        }
    }

public:
    SerialSource(const char* filename = "/dev/ttyUSB0") {
        file = std::fopen(filename, "r");
        eof = false;
        currentValue = 0;
        if (file == NULL) {
            std::printf("ERROR: No se pudo abrir el puerto serial '%s'.\n", filename);
            eof = true;
        } else {
            readNextInternal();
        }
    }

    virtual ~SerialSource() {
        if (file != NULL) {
            std::fclose(file);
            file = NULL;
        }
    }

    virtual int getNext() {
        int value = currentValue;
        readNextInternal();
        return value;
    }

    virtual bool hasMoreData() {
        return !eof;
    }
};

// ========================== FileSource (chunks .tmp) =========================

class FileSource : public DataSource {
private:
    FILE* file;
    bool eof;
    int currentValue;

    void readNextInternal() {
        if (file == NULL) {
            eof = true;
            return;
        }
        int temp;
        int result = std::fscanf(file, "%d", &temp);
        if (result == 1) {
            currentValue = temp;
            eof = false;
        } else {
            eof = true;
        }
    }

public:
    FileSource(const char* filename) {
        file = std::fopen(filename, "r");
        eof = false;
        currentValue = 0;
        if (file == NULL) {
            std::printf("ERROR: No se pudo abrir chunk '%s'.\n", filename);
            eof = true;
        } else {
            readNextInternal();
        }
    }

    virtual ~FileSource() {
        if (file != NULL) {
            std::fclose(file);
            file = NULL;
        }
    }

    virtual int getNext() {
        int value = currentValue;
        readNextInternal();
        return value;
    }

    virtual bool hasMoreData() {
        return !eof;
    }
};

// ========================== Lista Circular =============================

struct Node {
    int value;
    Node* next;
    Node* prev;
};

class CircularBuffer {
private:
    Node* head;
    int count;
    int capacity;

public:
    CircularBuffer(int cap) {
        head = NULL;
        count = 0;
        capacity = cap;
    }

    ~CircularBuffer() {
        clear();
    }

    bool isFull() const { return count >= capacity; }
    bool isEmpty() const { return count == 0; }
    int size() const { return count; }

    void insert(int value) {
        if (isFull()) return;

        Node* node = new Node;
        node->value = value;

        if (head == NULL) {
            head = node;
            head->next = head;
            head->prev = head;
        } else {
            Node* tail = head->prev;
            tail->next = node;
            node->prev = tail;
            node->next = head;
            head->prev = node;
        }
        count++;
    }

    void toArray(int* dest) const {
        if (head == NULL) return;
        Node* current = head;
        for (int i = 0; i < count; ++i) {
            dest[i] = current->value;
            current = current->next;
        }
    }

    void clear() {
        if (head == NULL) {
            count = 0;
            return;
        }
        Node* current = head;
        for (int i = 0; i < count; ++i) {
            Node* next = current->next;
            delete current;
            current = next;
        }
        head = NULL;
        count = 0;
    }
};

// ========================== Insertion Sort ==================================

void insertionSort(int* arr, int n) {
    for (int i = 1; i < n; ++i) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// ========================== Escribir chunk ===================================

int escribirChunkDesdeBuffer(CircularBuffer& buffer, int chunkIndex) {
    int n = buffer.size();
    if (n == 0) return 0;

    int* datos = new int[n];
    buffer.toArray(datos);

    insertionSort(datos, n);

    char filename[64];
    std::sprintf(filename, "chunk_%d.tmp", chunkIndex);

    FILE* f = std::fopen(filename, "w");
    if (f == NULL) {
        delete[] datos;
        return 0;
    }

    for (int i = 0; i < n; ++i)
        std::fprintf(f, "%d\n", datos[i]);

    std::fclose(f);
    delete[] datos;

    buffer.clear();
    return 1;
}

// ========================== K-Way Merge ======================================

void fusionExternaKWay(int numChunks, const char* outputFilename) {
    if (numChunks <= 0) {
        std::printf("No hay chunks.\n");
        return;
    }

    DataSource** fuentes = new DataSource*[numChunks];
    bool* activa = new bool[numChunks];
    int* valores = new int[numChunks];

    for (int i = 0; i < numChunks; ++i) {
        char filename[64];
        std::sprintf(filename, "chunk_%d.tmp", i);

        fuentes[i] = new FileSource(filename);
        if (fuentes[i]->hasMoreData()) {
            valores[i] = fuentes[i]->getNext();
            activa[i] = true;
        } else activa[i] = false;
    }

    FILE* out = std::fopen(outputFilename, "w");
    if (out == NULL) {
        for (int i = 0; i < numChunks; ++i) delete fuentes[i];
        delete[] fuentes; delete[] activa; delete[] valores;
        return;
    }

    while (true) {
        int minIndex = -1;
        int minValue = 0;

        for (int i = 0; i < numChunks; ++i) {
            if (!activa[i]) continue;
            if (minIndex == -1 || valores[i] < minValue) {
                minIndex = i;
                minValue = valores[i];
            }
        }

        if (minIndex == -1) break;

        std::fprintf(out, "%d\n", minValue);

        if (fuentes[minIndex]->hasMoreData())
            valores[minIndex] = fuentes[minIndex]->getNext();
        else
            activa[minIndex] = false;
    }

    std::fclose(out);

    for (int i = 0; i < numChunks; ++i) delete fuentes[i];
    delete[] fuentes; delete[] activa; delete[] valores;
}

// ========================== main =============================================

int main() {

    const int BUFFER_CAPACITY = 4;
    const int MAX_READS = 100;
    const char* OUTPUT_FILENAME = "output.sorted.txt";

    std::printf("Iniciando E-Sort con Arduino real...\n");

    SerialSource serial("/dev/ttyUSB0");

    CircularBuffer buffer(BUFFER_CAPACITY);
    int chunkIndex = 0;
    int reads = 0;

    std::printf("Iniciando Fase 1: Adquisición de datos...\n");

    while (serial.hasMoreData() && reads < MAX_READS) {
        int value = serial.getNext();
        std::printf("Leyendo -> %d\n", value);
        buffer.insert(value);
        reads++;

        if (buffer.isFull()) {
            std::printf("Buffer lleno. Ordenando internamente...\n");
            if (escribirChunkDesdeBuffer(buffer, chunkIndex)) {
                std::printf("Escribiendo chunk_%d.tmp... OK.\n", chunkIndex);
                chunkIndex++;
            }
        }
    }

    if (!buffer.isEmpty()) {
        std::printf("Buffer con datos residuales. Escribiendo último chunk...\n");
        if (escribirChunkDesdeBuffer(buffer, chunkIndex)) {
            std::printf("Escribiendo chunk_%d.tmp... OK.\n", chunkIndex);
            chunkIndex++;
        }
    }

    std::printf("Fase 1 completada. %d chunks generados.\n", chunkIndex);

    std::printf("Iniciando Fase 2: Fusión Externa (K-Way Merge)...\n");
    fusionExternaKWay(chunkIndex, OUTPUT_FILENAME);

    std::printf("Fusión completada. Archivo final: %s\n", OUTPUT_FILENAME);
    std::printf("Liberando memoria... Sistema apagado.\n");

    return 0;
}
