#include "constants.h"
#include "image_utils.hpp"
#include "gaussian_blur.hpp"

/**************************************************************************/

int main(int argc, char** argv )  {

    /* Se checkean los parametros de entrada */
    if (argc != PARAMS) {
    	printf("Inserte los parametros necesarios...\n");
	    exit(-1);
    }

    /* Se mide tiempo de ejecución */
    clock_t start_time = clock();

    int kernel_size = atoi(argv[1]);
    double _sigma = atol(argv[2]);
    Matrix _kernel = calc_kernel(kernel_size, kernel_size, _sigma);
    Image _img = load_image(argv[3]);
    img_height = _img[0].size();
    img_width = _img[0][0].size();

    MPI_Init(&argc, &argv);
    int p, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    unsigned char arr_img_comp[img_height*img_width];
    const int NPROWS = 1; 
    const int NPCOLS = p;  
    const int BLOCKROWS = (img_height / NPROWS); 
    const int BLOCKCOLS = (img_width / NPCOLS);

    // Llena matriz con pixeles de la imagen
    if (rank == 0) {
        matrix_from_image(_img, arr_img_comp, img_width, img_height);
    }

    /* Verifica si se divide correctamente la imagen según la cantidad de núcleos */
    if (p != NPROWS*NPCOLS) {
        fprintf(stderr,"Error: number of processors %d != %d x %d\n", p, NPROWS, NPCOLS);
        MPI_Finalize();
        exit(-1);
    }

    /* Prepara los bloques de matriz */
    unsigned char arr_img_block[BLOCKROWS*BLOCKCOLS];
    for (int ii=0; ii<BLOCKROWS*BLOCKCOLS; ii++) { arr_img_block[ii] = 0; }

    MPI_Datatype blocktype;
    MPI_Datatype blocktype2;

    MPI_Type_vector(BLOCKROWS, BLOCKCOLS, img_width, MPI_CHAR, &blocktype2);
    MPI_Type_create_resized(blocktype2, 0, sizeof(char), &blocktype);
    MPI_Type_commit(&blocktype);

    int disps[NPROWS*NPCOLS];
    int counts[NPROWS*NPCOLS];
    for (int ii=0; ii<NPROWS; ii++) {
        for (int jj=0; jj<NPCOLS; jj++) {
            disps[ii*NPCOLS+jj] = ii*img_width*BLOCKROWS+jj*BLOCKCOLS;
            counts [ii*NPCOLS+jj] = 1;
        }
    }

    /* Divide la matriz según la cantidad de nodos y separa el trabajo */
    MPI_Scatterv(arr_img_comp, counts, disps, blocktype, arr_img_block,
                 BLOCKROWS*BLOCKCOLS,  MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    Image gauss_img(RGB, Matrix(BLOCKROWS, Array(BLOCKCOLS)));
    image_from_matrix(arr_img_block, gauss_img, BLOCKCOLS, BLOCKROWS);

    Image filtered(RGB, Matrix(BLOCKROWS, Array(BLOCKCOLS)));

    for (int proc = 0; proc < p; proc++) {
        if (proc == rank) {
            filtered = apply_gaussian_filter(gauss_img, _kernel);
        }

        matrix_from_image(filtered, arr_img_block, filtered[0][0].size(), filtered[0].size());
        MPI_Barrier(MPI_COMM_WORLD);
    }

    /* Rearma la matriz despues de haber sido distribuida a los nodos */
    MPI_Gatherv(arr_img_block, BLOCKROWS*BLOCKCOLS,  MPI_UNSIGNED_CHAR, arr_img_comp, counts,
                 disps, blocktype, 0, MPI_COMM_WORLD);

    /* Muestra resultado de la imagen filtrada */
    Image result_image(RGB, Matrix(img_height, Array(img_width)));
    image_from_matrix(arr_img_comp, result_image, img_width, img_height);
    save_image(result_image, argv[4]);
    MPI_Finalize();

    clock_t end_time = clock();
    double _duration = double(end_time - start_time) / CLOCKS_PER_SEC;
    std::cout << "Tiempo de ejecución en rank " << rank << ": " << _duration << "s" << std::endl;
    return 0;
}
