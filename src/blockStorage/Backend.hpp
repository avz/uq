#ifndef BLOCKSTORAGE_BACKEND_HPP
#define	BLOCKSTORAGE_BACKEND_HPP

namespace blockStorage {
	class Backend {
	public:
		/**
		* Прочитать данные из хранилища. Возыращаемый указатель должен быть
		* явно осовобождён методом free()
		* @param size
		* @param offset
		* @return указатель на buf
		*/
		void *read(off_t offset, ssize_t size);
		void write(const void *buf, off_t offset, ssize_t size);

		/**
		 * Освободить буффер, выделенный read()
         * @param buf
         */
		void free(void *buf);

		/**
		 * Дополнительно аллоцировать size байт.
		 * После аллокации блок может быть write() или read().
         * @param size
         * @return смещение нового блока
         */
		off_t allocate(ssize_t size);

		off_t size();
		void clear();
	};
}

#endif	/* BLOCKSTORAGE_BACKEND_HPP */

